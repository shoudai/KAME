#define msecsleep(x) (x)

//#include "xtime.h"

#include "support.h"

#include <stdint.h>

//
//#ifndef HAVE_CAS_2
//inline bool atomicCompareAndSet2(
//    uint32_t oldv0, uint32_t oldv1,
//    uint32_t newv0, uint32_t newv1, uint32_t *target ) {
//        ASSERT(oldv0 == target[0]);
//        ASSERT(oldv1 == target[1]);
//        if(rand() > RAND_MAX/2) {
//            target[0] = newv0;
//            target[1] = newv1;
//            return true;
//        }
//        return false;
//    }
//#endif

#include "atomic_smart_ptr.h"
#include "thread.cpp"

int objcnt = 0;

class A {
public:
	A(int x) : m_x(x) {
//		fprintf(stdout, "c", x);
        atomicInc(&objcnt);
	}
	virtual ~A() {
//		fprintf(stdout, "d", m_x);
        atomicDec(&objcnt);
	}
    virtual int x() const {return m_x;}

int m_x;
};
class B : public A {
public:
    B(int x) : A(x) {
//        fprintf(stdout, "C");
    }
    ~B() {
//        fprintf(stdout, "D");
    }
    virtual int x() const {return -m_x;}
    virtual int xorg() const {return m_x;}
};


atomic_scoped_ptr<A> gp1, gp2, gp3;

void *
start_routine(void *) {
	for(int i = 0; i < 100000; i++) {
    	atomic_scoped_ptr<A> p1(new A(1));
    	atomic_scoped_ptr<A> p2(new B(2));
    	atomic_scoped_ptr<A> p3;


    	p2.swap(gp1);
    	gp2.reset(new A(51));
    	gp3.reset(new A(3));

    	gp3.reset();
    	p2.swap(p3);

        usleep(10);
	}
    return 0;
}

#define NUM_THREADS 4

int
main(int argc, char **argv)
{
    timeval tv;
    gettimeofday(&tv, 0);
    srand(tv.tv_usec);

pthread_t threads[NUM_THREADS];
	for(int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, start_routine, NULL);
	}
	for(int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	gp1.reset();
	gp2.reset();
	gp3.reset();
    if(objcnt != 0) {
    	printf("failed\n");
    	return -1;
    }
	printf("succeeded\n");
	return 0;
}