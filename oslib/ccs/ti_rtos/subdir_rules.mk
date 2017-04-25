################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
osi_tirtos.obj: C:/projects/cc3200/oslib/osi_tirtos.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv6/tools/compiler/ti-cgt-arm_5.2.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -Ooff --include_path="C:/projects/cc3200/oslib" --include_path="C:/projects/cc3200/oslib/" --include_path="C:/ti/ccsv6/tools/compiler/ti-cgt-arm_5.2.5/include" -g --gcc --define=cc3200 --define=ccs --diag_wrap=off --diag_warning=225 --display_error_number --printf_support=full --ual --preproc_with_compile --preproc_dependency="osi_tirtos.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


