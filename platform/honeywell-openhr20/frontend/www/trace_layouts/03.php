<?php

$trace_layout_ids_double = array (
    array( 'temp_average' , 'average temperature [1/100 degree C]' ),
    array( 'bat_average' , 'battery voltage [1/1000 V]' ),
    array( 'sumError_LO_W' , '' ),
    array( 'sumError_HI_W' , '' ),
    array( 'CTL_temp_wanted' , '' ),
    array( 'CTL_temp_wanted_last' , '' ),
    array( 'CTL_temp_auto' , '' ),
    array( 'CTL_mode_auto' , '' ),
    array( 'CTL_mode_window' , 'Controller mode window timeout (0=closed)' ),
    array( 'motor_diag' , 'MOTOR diagnostic, time between 2 pulses' ),
    array( 'MOTOR_PosMax' , 'MOTOR maximum position [pulses]' ),
    array( 'MOTOR_PosAct' , 'MOTOR actual position [pulses]' ),
    array( 'MOTOR_PosOvershoot' , 'volume of pulses after last motor stop'),
    0xff => array( 'LAYOUT_VERSION' , '' )
);

foreach ($trace_layout_ids_double as $k=>$v) {
  $trace_layout_ids[$k]=$v[0];
  $trace_layout_names[$v[0]]=$k;
}
