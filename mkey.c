#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>

enum{
	Nrate = 44100,
	Ndelay = Nrate / 50,
};
char *notetab[] = {
	"c0","c♯0","d0","d♯0","e0","f0","f♯0","g0","g♯0","a0","a♯0","b0",
	"c1","c♯1","d1","d♯1","e1","f1","f♯1","g1","g♯1","a1","a♯1","b1",
	"c2","c♯2","d2","d♯2","e2","f2","f♯2","g2","g♯2","a2","a♯2","b2",
	"c3","c♯3","d3","d♯3","e3","f3","f♯3","g3","g♯3","a3","a♯3","b3",
	"c4","c♯4","d4","d♯4","e4","f4","f♯4","g4","g♯4","a4","a♯4","b4",
	"c5","c♯5","d5","d♯5","e5","f5","f♯5","g5","g♯5","a5","a♯5","b5",
	"c6","c♯6","d6","d♯6","e6","f6","f♯6","g6","g♯6","a6","a♯6","b6",
	"c7","c♯7","d7","d♯7","e7","f7","f♯7","g7","g♯7","a7","a♯7","b7",
	"c8","c♯8","d8","d♯8","e8","f8","f♯8","g8","g♯8","a8","a♯8","b8",
	"c9","c♯9","d9","d♯9","e9","f9","f♯9","g9","g♯9","a9","a♯9","b9",
	"c10","c♯10","d10","d♯10","e10","f10","f♯10","g10"
};
int oct0 = 3*12, vol = 63, freq[nelem(notetab)], fon[nelem(notetab)];
int soft;

Rune keytab[] = {
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', Kshift, '\n',
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\\',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='
};

void
athread(void *)
{
	int i, fd;
	short s;
	uvlong T;
	uchar out[4*Ndelay], *p;

	if((fd = open("/dev/audio", OWRITE)) < 0)
		sysfatal("open: %r");
	for(T=0;;){
		for(p=out; p<out+nelem(out); T++, p+=4){
			for(i=0, s=0; i<nelem(freq); i++)
				if(fon[i] && T % freq[i] >= freq[i] / 2)
					s += 64;
			s *= vol;
			p[0] = p[2] = s;
			p[1] = p[3] = s >> 8;
		}
		if(write(fd, out, sizeof out) != sizeof out)
			break;
		yield();
	}
	close(fd);
}

void
kproc(void *)
{
	int n, fh, fd, kon;
	char buf[128], buf2[128], *s, *s2;
	uchar u[4];
	Rune r, *k;

	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("open: %r");
	memset(buf, 0, sizeof buf);
	memset(buf2, 0, sizeof buf2);
	fh = font->height * (nelem(keytab) / 12 - 1);
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			if((n = read(fd, buf, sizeof(buf)-1)) < 0)
				sysfatal("read: %r");
			buf[n-1] = 0;
			buf[n] = 0;
		}
		if(buf[0] == 'c'){
			if(utfrune(buf+1, Kdel))
				goto end;
			if(utfrune(buf+1, KF|1) && oct0 >= 12)
				oct0 -= 12;
			if(utfrune(buf+1, KF|2) && oct0 < 12*7)
				oct0 += 12;
			if(utfrune(buf+1, KF|3) && vol > 0)
				vol -= 7;
			if(utfrune(buf+1, KF|4) && vol < 126)
				vol += 7;
		}
		kon = buf[0] == 'k';
		if(kon){
			s = buf+1;
			s2 = buf2+1;
		}else if(buf[0] == 'K'){
			s = buf2+1;
			s2 = buf+1;
		}else
			continue;
		lockdisplay(display);
		while(*s != 0){
			s += chartorune(&r, s);
			if(r == Kdel)
				goto end;
			if(utfrune(s2, r) != nil)
				continue;
			for(k=keytab; k<keytab+nelem(keytab); k++){
				if(r != *k)
					continue;
				n = k - keytab;
				if(n+oct0 >= nelem(notetab))
					break;
				string(screen, addpt(screen->r.min, Pt(n%12*4*font->width, fh-n/12*font->height)),
					 kon ? display->white : display->black,
					 ZP, font, notetab[n+oct0]);
				if(soft)
					fon[n+oct0] = kon;
				else{
					u[0] = 0;
					u[1] = kon ? 0x90 : 0x80;
					u[2] = n+oct0;
					u[3] = kon ? vol : 0;
					write(1, u, sizeof u);
				}
			}
		}
		flushimage(display, 1);
		unlockdisplay(display);
		strcpy(buf2, buf);
	}
end:
	close(fd);
	threadexitsall(nil);
}

void
usage(void)
{
	fprint(2, "usage: %s [-s]\n", argv0);
	threadexits("usage");
}

void
threadmain(int argc, char **argv)
{
	int i;
	Mousectl *mctl;

	ARGBEGIN{
	case 's': soft = 1; break;
	default: usage();
	}ARGEND
	if(initdraw(nil, nil, "mkey") < 0)
		sysfatal("initdraw: %r");
	string(screen, divpt(screen->r.max, 2), display->black, ZP, font, "a");	/* so font->width != 0... */
	draw(screen, screen->r, display->black, nil, ZP);
	flushimage(display, 1);
	display->locking = 1;
	unlockdisplay(display);
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if(soft){
		for(i=0; i<nelem(freq); i++)
			freq[i] = Nrate / (440 * pow(1.05946, i - 69));
		if(threadcreate(athread, nil, mainstacksize) < 0)
			sysfatal("threadcreate: %r");
	}
	if(proccreate(kproc, nil, mainstacksize) < 0)
		sysfatal("proccreate: %r");
	Alt a[] = {
		{mctl->resizec, nil, CHANRCV},
		{mctl->c, nil, CHANRCV},
		{nil, nil, CHANEND}
	};
	for(;;)
		if(alt(a) == 0){
			lockdisplay(display);
			if(getwindow(display, Refnone) < 0)
				sysfatal("resize failed: %r");
			draw(screen, screen->r, display->black, nil, ZP);
			flushimage(display, 1);
			unlockdisplay(display);
		}
}
