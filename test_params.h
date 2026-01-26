/**
 * @file test_params.h
 * @brief PCR test parameter structure
 */

#ifndef TEST_PARAMS_H
#define TEST_PARAMS_H

struct TestParams {
  int Temp_Init_Denat = 95;
  int Time_Init_Denat = 120;
  int Temp_Denat = 95;
  int Time_Denat = 10;
  int Temp_Anneal = 60;
  int Time_Anneal = 20;
  int Temp_Extension = 72;
  int Time_Extension = 20;
  int Num_Cycles = 45;
  int Temp_Final_Ext = 72;
  int Time_Final_Ext = 240;
};

#endif // TEST_PARAMS_H
