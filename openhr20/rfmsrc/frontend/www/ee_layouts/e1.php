<?php

$layout_ids_double = array (
    array( 'lcd_contrast' , '' ),
    array( 'temperature0' , 'temperature 0  - frost protection (unit is 0.5stC)' ),
    array( 'temperature1' , 'temperature 1  - energy save (unit is 0.5stC)' ),
    array( 'temperature2' , 'temperature 2  - comfort (unit is 0.5stC)' ),
    array( 'temperature3' , 'temperature 3  - supercomfort (unit is 0.5stC)' ),
    array( 'PP_Factor' , 'Proportional kvadratic tuning constant, multiplied with 256' ),
    array( 'P_Factor' , 'Proportional tuning constant, multiplied with 256' ),
    array( 'I_Factor' , 'Integral tuning constant, multiplied with 256' ),
    array( 'temp_tolerance' , 'tolerance of temperature in 1/100 degree to lazy integrator (improve stability)' ),
    array( 'PID_interval' , 'PID_interval*5 = interval in seconds' ),
    array( 'valve_min' , 'valve position limiter min' ),
    array( 'valve_center' , 'default valve position for "zero - error" - improve stabilization after change temperature' ),
    array( 'valve_max' , 'valve position limiter max' ),
    array( 'motor_pwm_min' , 'min PWM for motor' ),
    array( 'motor_pwm_max' , 'max PWM for motor' ),
    array( 'motor_eye_low' , 'min signal lenght to accept low level (multiplied by 2)' ),
    array( 'motor_eye_high' , 'min signal lenght to accept high level (multiplied by 2)' ),
    array( 'motor_end_detect_cal' , 'stop timer threshold in % to previous average' ),
    array( 'motor_end_detect_run' , 'stop timer threshold in % to previous average' ),
    array( 'motor_speed' , '/8' ),
    array( 'motor_speed_ctl_gain' , '' ),
    array( 'motor_pwm_max_step' , '' ),
    array( 'MOTOR_ManuCalibration_L' , '' ),
    array( 'MOTOR_ManuCalibration_H' , '' ),
    array( 'temp_cal_table0' , 'temperature calibration table' ),
    array( 'temp_cal_table1' , 'temperature calibration table' ),
    array( 'temp_cal_table2' , 'temperature calibration table' ),
    array( 'temp_cal_table3' , 'temperature calibration table' ),
    array( 'temp_cal_table4' , 'temperature calibration table' ),
    array( 'temp_cal_table5' , 'temperature calibration table' ),
    array( 'temp_cal_table6' , 'temperature calibration table' ),
    array( 'timer_mode' , '=0 only one program, =1 programs for weekdays' ),
    array( 'bat_warning_thld' , 'treshold for battery warning [unit 0.02V]=[unit 0.01V per cell]' ),
    array( 'bat_low_thld' , 'threshold for battery low [unit 0.02V]=[unit 0.01V per cell]' ),
    array( 'window_open_thld' , 'threshold for window open/close detection unit is 0.1C' ),
    array( 'window_open_noise_filter' , '' ),
    array( 'window_close_noise_filter' , '' ),
    array( 'RFM_devaddr' , "HR20's own device address in RFM radio networking. =0 mean disable radio"),
    array( 'security_key0' , 'key for encrypted radio messasges' ),
    array( 'security_key1' , 'key for encrypted radio messasges' ),
    array( 'security_key2' , 'key for encrypted radio messasges' ),
    array( 'security_key3' , 'key for encrypted radio messasges' ),
    array( 'security_key4' , 'key for encrypted radio messasges' ),
    array( 'security_key5' , 'key for encrypted radio messasges' ),
    array( 'security_key6' , 'key for encrypted radio messasges' ),
    array( 'security_key7' , 'key for encrypted radio messasges' ),
    0xff => array( 'LAYOUT_VERSION' , '' )

);

foreach ($layout_ids_double as $k=>$v) {
  $layout_ids[$k]=$v[0];
  $layout_names[$v[0]]=$k;
}
