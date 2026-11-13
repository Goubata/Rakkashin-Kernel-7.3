Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, drivers/soc/qcom/llcc-qcom.c.
Alor firmware tables retained. Public 7.3 llcc_slice_desc and functions use target types;
private hardware/EDAC structures use avr_* names. Firmware use-case IDs have
AVR_ names; GPU/GPUHTW/GPUMV IDs are compile-time checked against target IDs.
The vendor EDAC/perfmon consumers must use this private layout when ported;
do not pair this provider with the upstream EDAC driver's private-data layout.
Hardware operation is unverified.
