TMP_FILE="./tmp.c"

csmith -o $TMP_FILE \
  --no-argc \
  --no-checksum \
  --no-bitfields \
  --inline-function \
  --no-structs \
  --no-volatiles \
  --no-unions \
  --no-global-variables \
  --no-builtins \
  --no-math64 \
  --no-uint8 \
  --no-int8 \
  --no-pointers \
  --no-safe-math \
  --max-funcs 1 \
  || exit

bash ./compile.sh $TMP_FILE
