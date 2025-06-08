int main(void) {
	int x = 1, y = 2;
	int a = (int)'a';
	char b = (const char)x;
	a = (const int)'a';
	b = (char)((const int)300 + (int)'a');
	a = (int)(char)(const int)(const char)(x + y);
	return 0;
}