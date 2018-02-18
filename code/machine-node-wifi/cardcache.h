#ifndef cardcache_h
#define cardcache_h

#include "Arduino.h"

class CardCache {
        public:
                CardCache (String file);
                int delUid (String uid);
                unsigned long doesUidExist (String uid);
                bool addUid (String uid);
                void setup (bool format = false);
                bool clearCache ();
        private:
                String _file;
                bool writeUid (String uid);
                bool scrubPosition (unsigned long pos);
};

#endif


