Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, W3WWL36.65-11-18.
Source: drivers/soc/qcom/socinfo.c.
Build against the actual 7.3 public/private framework types; do not load on running 6.12. Hardware operation unverified.
The target public socinfo struct reserves the same four trailing u32 fields.
Vendor format-21/22 offset reads use reserve1[0]/reserve1[1] explicitly; no old
public core structure is overlaid. Firmware/header/array bounds and remove-time
publication still need a broader audit before claiming production robustness.
