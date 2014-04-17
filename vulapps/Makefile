all: sendmail bind wu-ftpd

sendmail: s1 s2 s3 s4 s5 s6 s7

bind:	b1 b2 b3 b4

wu-ftpd: f1 f2 f3

s1: 
	cd sendmail/s1; \
	make; \
	cd ../..
s2: 
	cd sendmail/s2; \
	make; \
	cd ../..
s3: 
	cd sendmail/s3; \
	make; \
	cd ../..
s4: 
	cd sendmail/s4; \
	make; \
	cd ../..
s5: 
	cd sendmail/s5; \
	make; \
	cd ../..
s6: 
	cd sendmail/s6; \
	make; \
	cd ../..
s7: 
	cd sendmail/s7; \
	make; \
	cd ../..


b1: 
	cd bind/b1; \
	make; \
	cd ../..
b2: 
	cd bind/b2; \
	make; \
	cd ../..
b3: 
	cd bind/b3; \
	make; \
	cd ../..
b4: 
	cd bind/b4; \
	make; \
	cd ../..

f1: 
	cd wu-ftpd/f1; \
	make; \
	cd ../..
f2: 
	cd wu-ftpd/f2; \
	make; \
	cd ../..
f3: 
	cd wu-ftpd/f3; \
	make; \
	cd ../..


clean:
	rm runAll.out ; \
	cd sendmail ; \
	cd s1; make clean ; cd .. ; \
	cd s2; make clean ; cd .. ; \
	cd s3; make clean ; cd .. ; \
	cd s4; make clean ; cd .. ; \
	cd s5; make clean ; cd .. ; \
	cd s6; make clean ; cd .. ; \
	cd s7; make clean ; cd .. ; \
	cd ../bind ; \
	cd b1; make clean ; cd .. ; \
	cd b2; make clean ; cd .. ; \
	cd b3; make clean ; cd .. ; \
	cd b4; make clean ; cd .. ; \
	cd ../wu-ftpd ; \
	cd f1; make clean ; cd .. ; \
	cd f2; make clean ; cd .. ; \
	cd f3; make clean 

