# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\workspace\ethernet\qtproto3\vitis\zynq\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\workspace\ethernet\qtproto3\vitis\zynq\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {zynq}\
-hw {C:\workspace\ethernet\qtproto3\docs\zynq.xsa}\
-fsbl-target {psu_cortexa53_0} -out {C:/workspace/ethernet/qtproto3/vitis}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {lwip_udp_perf_client}
platform generate -domains 
platform active {zynq}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick
bsp reload
bsp reload
