int main(int argc, char **argv) {
  if (argc < 3)
    return 1;
  int a = (int)argv[1][0] + (int)argv[1][1];
  int b = (int)argv[2][0];
  for (int i = 0; i < a; i++) {
    b += a * b;
  }
  for (int i = 0; i < b; i++) {
    a += b;
  }
  return a * b;
}
