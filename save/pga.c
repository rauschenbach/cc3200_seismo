#if 0
	/* ��������: PGA �������� ��� 4-� �������. �������� �����, 
	 * �������������� ��������.��������� � ���� ������� */
	if(adcp.adc_board_1.ch_1_gain == 12){
		tmp = PGA12;
	} else if(adcp.adc_board_1.ch_1_gain == 8){
		tmp = PGA8;
	} else if(adcp.adc_board_1.ch_1_gain == 4){
		tmp = PGA4;
	} else if(adcp.adc_board_1.ch_1_gain == 2){
		tmp = PGA2;
	} else {
		tmp = PGA1;
	}
	par.pga[0] = (ADS131_PgaEn)tmp;


	/* ������ ����� */
	if(adcp.adc_board_1.ch_2_gain == 12){
		tmp = PGA12;
	} else if(adcp.adc_board_1.ch_2_gain == 8){
		tmp = PGA8;
	} else if(adcp.adc_board_1.ch_2_gain == 4){
		tmp = PGA4;
	} else if(adcp.adc_board_1.ch_2_gain == 2){
		tmp = PGA2;
	} else {
		tmp = PGA1;
	}
	par.pga[1] = (ADS131_PgaEn)tmp;

	/* ������ ����� */
	if(adcp.adc_board_1.ch_3_gain == 12){
		tmp = PGA12;
	} else if(adcp.adc_board_1.ch_3_gain == 8){
		tmp = PGA8;
	} else if(adcp.adc_board_1.ch_3_gain == 4){
		tmp = PGA4;
	} else if(adcp.adc_board_1.ch_3_gain == 2){
		tmp = PGA2;
	} else {
		tmp = PGA1;
	}
	par.pga[2] = (ADS131_PgaEn)tmp;


	/* ��������� ����� */
	if(adcp.adc_board_1.ch_3_gain == 12){
		tmp = PGA12;
	} else if(adcp.adc_board_1.ch_3_gain == 8){
		tmp = PGA8;
	} else if(adcp.adc_board_1.ch_3_gain == 4){
		tmp = PGA4;
	} else if(adcp.adc_board_1.ch_3_gain == 2){
		tmp = PGA2;
	} else {
		tmp = PGA1;
	}
#endif
