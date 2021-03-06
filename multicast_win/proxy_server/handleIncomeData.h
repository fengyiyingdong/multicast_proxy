#ifndef _HANDLEINCOMEDATA_H_
#define _HANDLEINCOMEDATA_H_

extern unsigned int grecv_pkt;
extern unsigned int gdrop_pkt;
extern unsigned int gforward_pkt; 
extern unsigned int gforward_point_pkt; 

/*
 * func: 	dealing with the data coming.
 * param: 	pBuf points to the coming data.
 *			bufLen is the length of data in buf.
 * return: 	0 success; -1 fail
 */
int HandleIncomingData(UCHAR* pBuf, int bufLen, int type);
#endif
