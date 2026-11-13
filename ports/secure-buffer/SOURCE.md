Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, drivers/soc/qcom/secure_buffer.c and trace_secure_buffer.h.
Batched ownership transfer and rollback logic are retained. SCM batches are
marshalled through the target TZMEM pool; do not split them into independent
region assignments. Protected ownership/rollback and device lifetime on avr
remain unverified. No privileged firmware calls are run on the phone in this phase.
