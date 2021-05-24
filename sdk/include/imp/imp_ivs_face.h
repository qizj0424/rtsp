/*
 * IMP IVS Face func header file.
 *
 * Copyright (C) 2016 Ingenic Semiconductor Co.,Ltd
 */

#ifndef __IMP_IVS_FACE_H__
#define __IMP_IVS_FACE_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */


#include <imp/imp_ivs.h>

/**
 * @file
 * IMP IVS 人脸识别模块
 */

/**
 * @defgroup FaceDetection
 * @ingroup IMP_IVS
 * @brief 人脸识别接口
 * @{
 */

/**
 * 人脸识别算法的输入结构体
 */
typedef struct {
	unsigned int width;
	unsigned int hight;
	unsigned int skipFrameCnt;
	float        reliability;
} IMP_IVS_FaceParam;

/*
 * 人脸识别算法的输出结构体
 */
#define MAX_COORNUM (64)
typedef struct {
	unsigned char out_info[128];
	unsigned int  x;
	unsigned int  y;
} FaceOutputInfo;

typedef struct {
	unsigned char  update;
	unsigned int   output_objnum;
	FaceOutputInfo output_info[MAX_COORNUM];
	//unsigned char out_info[128];
	//void *        out_result;				[>< 区域检测移动结果，与roiRect坐标信息严格对应 <]
	//unsigned int  out_width;
	//unsigned int  out_hight;
} IMP_IVS_FaceOutput;

/*
 * 人脸识别算法人脸坐标结构体
 */
typedef struct {
	unsigned int  x;
	unsigned int  y;
	unsigned int  w;
	unsigned int  h;
} IMP_IVS_FaceCoordinate;

typedef struct {
	unsigned int objnum;
	unsigned long long timestamp;
	IMP_IVS_FaceCoordinate coordinate[MAX_COORNUM];
} IMP_IVS_FaceMD;

/**
 * 创建人脸识别接口资源
 *
 * @fn IMPIVSInterface *IMP_IVS_CreateFaceInterface(IMP_IVS_FaceParam *param);
 *
 * @param[in] param 人脸识别算法的输入结构体参数
 *
 * @retval 非NULL 成功,返回人脸识别算法接口指针句柄
 * @retval NULL 失败
 *
 * @attention 无
 */
IMPIVSInterface *IMP_IVS_CreateFaceInterface(IMP_IVS_FaceParam *param);

unsigned int IMP_IVS_UpdateMd(IMP_IVS_FaceMD *face_coor);

/**
 * 销毁人脸识别接口资源
 *
 * @fn void IMP_IVS_DestroyMoveInterface(IMPIVSInterface *moveInterface);
 *
 * @param[in] moveInterface 人脸识别算法接口指针句柄
 *
 * @retval 无返回值
 *
 * @attention 无
 */
void IMP_IVS_DestroyFaceInterface(IMPIVSInterface *faceInterface);

/**
 * @}
 */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif /* __IMP_IVS_FACE_H__ */
