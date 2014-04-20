class Profile : public Object {
        String title;
        timeval start;
        public:
        Profile(const String& str="unknown") : title(str), start() {
                gettimeofday(&start,NULL);
        }
        
        ~Profile() {
                Logger log("Profiler");
                log.debug();
                try {
                        timeval stop,res;
                        gettimeofday(&stop,NULL);
                        timersub(&stop,&start,&res);
                        double temps = double(res.tv_sec)+double(res.tv_usec)/1000000.L;
                        log(title+"="+ftostring(temps)+"s");
                }
                catch (...) {
                        log("Exception occured in ~Profile()");
                }
        }
 
};
