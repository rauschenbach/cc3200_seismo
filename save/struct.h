#ifndef _STRUCT_H
#define _STRUCT_H

/***********************************************************************************
 * ���� ping_pong_flag == 0 - �������� ����, ���������� �� ����� ����
 * ���� ping_pong_flag == 1 - �������� �o��, ���������� �� ����� ����
 * ����������, ����� ����� ������ ������ ���� ��� ������ �������
 * ������ ���� ��� ������ ��� 32768 ����. � ��� ����� ����������:
 * 2 ������ ����������� ������� + ������  ���� ��� ������� ~10-20 ����� �� ��� ���������
 * ������ ������ PING ��� PONG ������ ���� ������ 4!
 * ����� ������� � ������ �.�. �����, ����� 60 �������� �� ���� ��� �������
 * ������ ������ PING ��� PONG - ������ MAX 12000 ����, 
 * ������ ������ PING ��� PONG - ������ �����:
 ***********************************************************************************/
static const ADS131_WORK_STRUCT adc_work_struct[] = {
//freq, decim,  bytes in pack, sec, num_in_pack, pack_size, period_us, sig in min, sig in hour
/**************************************************************************************************
 * 125 �� - ����� 1000 �� 8
 * 125 ��, ���=8,  3 ����/���, 30 ���  3750 ����. � 11250 ���� � �����, 8000 ���, 2 ���/���,  120/��� 
 * 125 ��, ���=8,  6 ����/���, 10 ���  1250 ����. � 7500 ���� � �����, 8000 ���, 6 ���/���, 360/��� 
 * 125 ��, ���=8,  9 ����/���, 10 ���  1250 ����. � 11250 ���� � �����, 8000 ���, 3 ���/���, 180/��� 
 * 125 ��, ���=8, 12 ����/���, 6 ���  750 ����. � 9000 ���� � �����, 8000 ���, 10 ���/���, 600/���
 **************************************************************************************************/
	{ SPS125, 8,  3, 30,  3750, 11250, 8000, 2,  120},//
	{ SPS125, 8,  6, 10,  1250, 7500, 8000, 6, 360},//	
	{ SPS125, 8,  9, 10,  1250, 11250, 8000, 6, 360},
	{ SPS125, 8, 12, 6,  750, 9000, 8000, 10, 600},

/**************************************************************************************************
 * 250 �� - ����� 2000 �� 8
 * 250 ��, ���=4,  3 ����/���, 15 ��� 3750 ����. � 11250 ���� � �����, 4000 ���,  4 ���/���, 240/��� 
 * 250 ��, ���=4,  6 ����/���, 6 ��� 1500 ����. � 9000 ���� � �����, 4000 ���,  10 ���/���, 600/��� 
 * 250 ��, ���=4,  9 ����/���, 5 ��� 1250 ����. � 11250 ���� � �����, 4000 ���,  12 ���/���, 720/��� 
 * 250 ��, ���=4, 12 ����/���, 3 ��� 750 ����. � 9000 ���� � �����, 4000 ���, 20 ���/���, 1200/���
 **************************************************************************************************/
	{ SPS250, 8,  3, 15, 3750, 11250, 4000,  3, 240}, // 
	{ SPS250, 8,  6, 6, 1500, 9000, 4000,  10, 600}, // 
	{ SPS250, 8,  9, 5, 1250, 11250, 4000,  12, 720}, // ok
	{ SPS250, 8, 12,  3, 750, 9000, 4000, 20, 1200}, // ok

/******************************************************************************************************
 * 500 �� - ����� 4000 �� 8
 * 500 ��, ���=2,  3 ����/���,  6 ��� 3000 ����. �  9000 ���� � �����, 2000 ���, 10 ���/���,  600/���
 * 500 ��, ���=2,  6 ����/���,  3 ��� 1500 ����. �  9000 ���� � �����, 2000 ���, 20 ���/���, 1800/���
 * 500 ��, ���=2,  9 ����/���,  2 ��� 1000 ����. �  9000 ���� � �����, 2000 ���, 30 ���/���, 1800/��� 
 * 500 ��, ���=2, 12 ����/���,  2 ��� 1000 ����. � 12000 ���� � �����, 2000 ���, 30 ���/���, 1800/��� 
 *****************************************************************************************************/
	{ SPS500, 8,  3,  6, 3000,  9000, 2000,  10, 600}, // 
	{ SPS500, 8,  6,  3, 1500,  9000, 2000, 20, 1800}, // 	
	{ SPS500, 8,  9,  2, 1000,  9000, 2000, 30, 1800}, // ok                        
	{ SPS500, 8, 12,  2, 1000, 12000, 2000, 30, 1800}, // ok

/*********************************************************************************************************
 * 1000 ��  
 * 1��� ���=0,  3 ����/���, ��� � 3 ���, 3000 ����. �  9000 ���� � �����, 1000 ���, 20 ���/���,  1200/��� 
 * 1��� ���=0,  6 ����/���, ��� � 2 ���, 2000 ����. � 12000 ���� � �����, 1000 ���, 30 ���/���, 1800/��� 
 * 1��� ���=0,  9 ����/���, ��� � 1 ���, 1000 ����. �  9000 ���� � �����, 1000 ���, 60 ���/���, 3600/��� 
 * 1��� ���=0, 12 ����/���, ��� � 1 ���, 1000 ����. � 12000 ���� � �����, 1000 ���, 60 ���/���, 3600/��� 
 *********************************************************************************************************/
	{ SPS1K, 1,  3,  3,  3000,  9000, 1000, 20,  1200},
	{ SPS1K, 1,  6,  2,  2000, 12000, 1000, 30,  1800},
	{ SPS1K, 1,  9,  1,  1000,  9000, 1000, 60, 3600}, 
	{ SPS1K, 1, 12,  1,  1000, 12000, 1000, 60, 3600}, 

/************************************************************************************************** 
 * 2000 �� - ����� ������ �������
 * 2���, ���=0,  3 ����/���,   2 ���  4000 ����,  12000 ���� � �����, 500 ���,  30 ���/���, 1800/���
 * 2���, ���=0,  6 ����/���,   1 ���  2000 ��c�,  12000 ���� � �����, 500 ���,  60 ���/���, 3600/��� 
 * 2���, ���=0,  9 ����/���,  1/2 ��� 1000 ����,  9000 ���� � �����, 500 ���,  120 ���/���, 7200/���
 * 2���, ���=0, 12 ����/���,  1/2 ��� 1000 ����, 12000 ���� � �����, 500 ���,  120 ���/���, 7200/���
***************************************************************************************************/
	{ SPS2K, 1,  3,  2,  4000, 12000,  500,   30, 1800},
	{ SPS2K, 1,  6,  1,  2000, 12000,  500,   60, 3600},
	{ SPS2K, 1,  9,  0,  1000,  9000,  500,  120, 7200},
	{ SPS2K, 1, 12,  0,  1000, 12000,  500,  120, 7200},

/**************************************************************************************************
 * 4000 �� - �� ����� ������������ � ������� ������ - ����� ������ �� ������ ������!
 * 4���, ���=0,  3 ����/���, 1   ��� 4000 ���� � 12000 ���� � �����, 250 ���, 60 ���/���,   3600/���
 * 4���, ���=0,  6 ����/���, 1/2 ��� 2000 ���� � 12000 ���� � �����, 250 ���, 120 ���/���,  7200/��� 
 * 4���, ���=0,  9 ����/���, 1/4 ��� 1000 ���� � 18000 ���� � �����, 250 ���, 240 ���/���, 14400/���
 * 4���, ���=0, 12 ����/���, 1/4 ��� 1000 ���� � 12000 ���� � �����, 250 ���, 240 ���/���, 14400/��� 
 *************************************************************************************************/
	{ SPS4K, 1,  3, 1,  4000, 12000, 250,  60,  3600}, 
	{ SPS4K, 1,  6, 0,  2000, 12000, 250, 120,  7200}, 
	{ SPS4K, 1,  9, 0,  1000,  9000, 250, 240, 14400}, 
	{ SPS4K, 1, 12, 0,  1000, 12000, 250, 240, 14400}, 
};

#endif /* struct.h  */
