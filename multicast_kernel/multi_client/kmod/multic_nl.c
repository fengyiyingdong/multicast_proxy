#include <linux/ip.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/version.h>
#include <linux/module.h>

#include "tmcc_nl.h"
#include "grp.h"

static struct sock * multi_nl_sk;

int multi_nl_node_add(struct tmcc_msg_st *msg_st)
{
    int ret;
    struct tmcc_nl_service_st *service;

    service = (struct tmcc_nl_service_st *)msg_st->data;

    ret = add_multi_node(service->multi_ip,service->server_ip, service->port);
    if(ret)
    {
        service->ret = ret;
        return ret;
    }

    service->ret = 0;
    return 0;
}

int multi_nl_node_delete(struct tmcc_msg_st *msg_st)
{
    struct tmcc_nl_service_st *service;

    service = (struct tmcc_nl_service_st *)msg_st->data;
    return del_multi_node(service->server_ip, service->port);
}

int multi_nl_node_clear(struct tmcc_msg_st *msg_st)
{
    return clear_multi_node();
}

static struct tmcc_nl_show_service_st  k_service;
int multi_nl_vm_list_get(struct tmcc_msg_st *msg_st)
{
    struct tmcc_nl_show_service_st __user *u_service;
    struct tmcc_nl_service_st  *service_in_kernel;
    int ret;

    service_in_kernel = (struct tmcc_nl_service_st *)msg_st->data;

    u_service = (struct tmcc_nl_show_service_st __user *)msg_st->reply_ptr;

    k_service.node_cnt = 0;

    ret = list_all_multi_grp(&k_service);
    k_service.ret = ret;

    if(copy_to_user(u_service, &k_service, sizeof(struct tmcc_nl_show_service_st)))
    {
        return -1;
    }

    return ret;
}

int multi_nl_node_packets_stats(struct tmcc_msg_st *msg_st)
{
    struct multic_packets_stats_st __user *u_stats;
    struct multic_packets_stats_st k_stats;
    struct tmcc_nl_service_st  *service_in_kernel;

    service_in_kernel = (struct tmcc_nl_service_st *)msg_st->data;

    u_stats = (struct multic_packets_stats_st __user *)msg_st->reply_ptr;

    memset(&k_stats, 0, sizeof(struct multic_packets_stats_st));

    if(service_in_kernel->server_ip == 0)
        get_all_multi_packets_stats(&k_stats);
    else 
        get_multi_packets_stats(&k_stats, service_in_kernel->server_ip, service_in_kernel->port);

    if(copy_to_user(u_stats, &k_stats, sizeof(struct multic_packets_stats_st)))
    {   
        return -1; 
    }   

    return 0;
}


void tmcc_rcv_skb(struct sk_buff *skb)
{
    int type, pid, flags, nlmsglen, skblen, ret = TMCC_OK;
    struct 	nlmsghdr *nlh;
    struct tmcc_msg_st *msg_st;

    skblen = skb->len;
    if (skblen < sizeof(*nlh)){
        return;
    }

#if  LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
    nlh = (struct nlmsghdr *)skb->data;
#else
    nlh = nlmsg_hdr(skb);
#endif
    nlmsglen = nlh->nlmsg_len;
    if (nlmsglen < sizeof(*nlh) || skblen < nlmsglen){
        return;
    }	

    pid = nlh->nlmsg_pid;
    flags = nlh->nlmsg_flags;

    if(flags & MSG_TRUNC){
        return;
    }

    type = nlh->nlmsg_type;

    msg_st = (struct tmcc_msg_st *)nlh;

    switch(type){
        case TMCC_SERVICE_ADD:
            ret = multi_nl_node_add(msg_st);
            break;
        case TMCC_SERVICE_DEL:
            ret = multi_nl_node_delete(msg_st);
            break;
        case MULTI_CLIENT_CLEAR:
            ret = multi_nl_node_clear(msg_st);
            break;
        case MULTIC_PACKETS_STATS:
            ret = multi_nl_node_packets_stats(msg_st);
            break;
        case TMCC_SERVICE_LIST:
            ret = multi_nl_vm_list_get(msg_st);
            break;
        default:
            break;
    }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,12,0)
    netlink_ack(skb, nlh, ret);
#else
    netlink_ack(skb, nlh, ret, NULL);
#endif
    return;
}

#if  LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
static DEFINE_MUTEX(rx_queue_mutex);

static void tmcc_rcv_sock(struct sock *sk, int len)
{
    struct sk_buff *skb;

    mutex_lock(&rx_queue_mutex);
    while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
        tmcc_rcv_skb(skb);
        kfree_skb(skb);
    }
    mutex_unlock(&rx_queue_mutex);
}
#endif

int multi_nl_init(void)
{
    /*netlink varies greatly in different kernel version, so in fact we only cover 2.6.32 and 3.10 kernel version.*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    struct netlink_kernel_cfg cfg = {
        .input  = tmcc_rcv_skb,
    };

    multi_nl_sk = netlink_kernel_create(&init_net, MULTI_NL, &cfg);
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
    multi_nl_sk = netlink_kernel_create(MULTI_NL, 0, tmcc_rcv_sock, THIS_MODULE);
#else
    multi_nl_sk = netlink_kernel_create(&init_net, MULTI_NL, 0, tmcc_rcv_skb, NULL, THIS_MODULE);
#endif
    if(multi_nl_sk == NULL){
        printk(KERN_ERR "tmcc_rcv_skb: failed to create netlink socket\n");
        return -1;
    }

    return 0;
}

void multi_nl_fini(void)
{
#if  LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
    sock_release(multi_nl_sk->sk_socket);
#else
    netlink_kernel_release(multi_nl_sk);
#endif
}
