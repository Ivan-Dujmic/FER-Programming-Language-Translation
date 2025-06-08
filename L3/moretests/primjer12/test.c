int bar(int x);
int foo(int a, int b) {
	if (a > 0) {
		return a + bar(b);
	} else {
		return 0;
	}
}
int bar(int a) {
	return foo(a, a-1);
}