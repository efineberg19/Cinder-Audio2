/*
Copyright notice taken from original package:
Copyright(C) 1996-2001 Takuya OOURA
email: ooura@mmm.t.u-tokyo.ac.jp
download: http://momonga.t.u-tokyo.ac.jp/~ooura/fft.html
You may use, copy, modify this code for any purpose and 
	without fee. You may distribute this ORIGINAL package.

*/

template <typename T> 
void cdft(int n, int isgn, T *a, int *ip, T *w);

template <typename T> 
void rdft(int n, int isgn, T *a, int *ip, T *w);

template <typename T> 
void ddct(int n, int isgn, T *a, int *ip, T *w);

template <typename T> 
void ddst(int n, int isgn, T *a, int *ip, T *w);

template <typename T> 
void dfct(int n, T *a, T *t, int *ip, T *w);

template <typename T> 
void dfst(int n, T *a, T *t, int *ip, T *w);
