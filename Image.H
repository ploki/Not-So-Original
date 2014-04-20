
#include <Magick++.h>


        DECLARE_EXCEPTION(ImageException,ServletException);
        DECLARE_EXCEPTION(VideoException,ServletException);


String convertRaw(const String& root, const String& tmp, const String& filename);
void makeThumbnail(const String& root, const String& tmp, const String& filename);
int getProcessDate(const String& filename);
bool isThumbnailUpToDate(const String& root, const String& tmp, const String& filename);
bool prepareThumbnail(const String& root, const String& tmp, const String& filename);
