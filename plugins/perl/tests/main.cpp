#include <string>
#include <iostream>

#include <EXTERN.h>
#include <perl.h>

#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

class interp {
public:
	interp() : my_perl(NULL){
		my_perl = perl_alloc();
		perl_construct(my_perl);
		
		const char* embedding[] = {"", "-e", "0"};
		perl_parse(my_perl, NULL, 3, (char**)embedding, NULL);
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
		perl_run(my_perl);
		
		eval_pv("sub test_method { my $a = @_[0]; $a = $a . \" and something else!\"; return $a; }", TRUE);
	};
	
	~interp() {
		perl_destruct(my_perl);
		perl_free(my_perl);
	};
	
	void run(const std::string& input) {
		std::string output;
		const char* a = input.c_str();
		{
			dSP;
			ENTER;
			SAVETMPS;
			
			PUSHMARK(SP);
			XPUSHs(sv_2mortal(newSVpv(a, 0)));
			
			PUTBACK;
			int count = call_pv("test_method", G_SCALAR);
			SPAGAIN;
			
			if (count > 0) {
				char* ptr = savepv(POPp);
				if (ptr) {
					output = std::string(ptr);
				}
			}
			
			FREETMPS;
			LEAVE;
		}
		
		std::cout << "in: " << input << std::endl;
		std::cout << "out: " << output << std::endl;
	}
	
	PerlInterpreter* my_perl;
};

bool stopping = false;

boost::mutex mutex;
boost::condition condition;

static void thread_proc_1() {
	interp it;
	
	while (!stopping) {
		/*
		boost::mutex::scoped_lock lock(mutex);
		boost::xtime t;
		boost::xtime_get(&t, boost::TIME_UTC);
		
		t.nsec += 10000000;
		condition.timed_wait(lock, t);
		*/
		it.run("thread 1");
	}
}

boost::mutex mutex2;
boost::condition condition2;

static void thread_proc_2() {
	interp it;
	
	while (!stopping) {
		/*
		boost::mutex::scoped_lock lock(mutex2);
		boost::xtime t;
		boost::xtime_get(&t, boost::TIME_UTC);
		
		t.nsec += 10000000;
		condition2.timed_wait(lock, t);
		*/
		it.run("thread 2");
	}
}

int main(int argc, char** argv) {
	PERL_SYS_INIT3(NULL, NULL, NULL);
	
	{
		boost::thread th1 = boost::thread(boost::bind(&thread_proc_1));
		boost::thread th2 = boost::thread(boost::bind(&thread_proc_2));
		
		sleep(1);

		stopping = true;
		th1.join();
		th2.join();
	}
	
	PERL_SYS_TERM();

	return 0;
}
