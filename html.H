/* 
 * Copyright (c) 2006-2011, Guillaume Gimenez <guillaume@blackmilk.fr>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of G.Gimenez nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL G.Gimenez SA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *     * Guillaume Gimenez <guillaume@blackmilk.fr>
 *
 */

class textfield {
  String title;
  String desc;
  String name;
  String value;
  public:
  textfield() : title(""), desc(""), name(""), value("") {}
  void setTitle(const String& str) { title=str; }
  void setDesc(const String& str) { desc=str; }
  void setName(const String& str) { name=str; }
  void setValue(const String& str) { value=str; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
    response << "<div class=\"row\">\n"
             << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
             << "  <div class=\"control\"><input type=\"text\" size=\"30\" name=\""<<name<<"\" value=\""<<value<<"\" /></div>\n"
             << "</div>\n";
  }
};

class password {
  String title;
  String desc;
  String name;
  String value;
  public:
  password() : title(""), desc(""), name(""), value("") {}
  void setTitle(const String& str) { title=str; }
  void setDesc(const String& str) { desc=str; }
  void setName(const String& str) { name=str; }
  void setValue(const String& str) { value=str; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
    response << "<div class=\"row\">\n"
             << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
             << "  <div class=\"control\"><input type=\"password\" size=\"30\" name=\""<<name<<"\" value=\""<<value<<"\" /></div>\n"
             << "</div>\n";
  }
};

class checkbox {
  String title;
  String desc;
  String name;
  bool checked;
  public:
  checkbox() : title(""), desc(""), name(""), checked(false) {}
  void setTitle(const String& str) { title=str; }
  void setDesc(const String& str) { desc=str; }
  void setName(const String& str) { name=str; }
  void setChecked(bool v) { checked=v; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
    response << "<div class=\"row\">\n"
             << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
             << "  <div class=\"control\"><input type=\"checkbox\" size=\"30\" name=\""<<name<<"\" "
             << (checked?"checked=\"checked\"":"") <<" /></div>\n"
             << "</div>\n";
  }
};
class textarea {
  String title;
  String desc;
  String name;
  String value;
  public:
  textarea() : title(""), desc(""), name(""), value("") {}
  void setTitle(const String& str) { title=str; }
  void setDesc(const String& str) { desc=str; }
  void setName(const String& str) { name=str; }
  void setValue(const String& str) { value=str; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
    response << "<div class=\"row\">\n"
             << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
             << "  <div class=\"control\"><textarea name=\""<<name<<"\" cols=\"40\" rows=\"10\">"<<value<<"</textarea></div>\n"
             << "</div>\n";
  }
};
class submit {
  String name;
  String value;
  public:
  submit() : name(""), value("") {}
  void setName(const String& str) { name=str; }
  void setValue(const String& str) { value=str; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
    response << "<div class=\"row\">\n"
             << "  <div class=\"label\"></div>\n"
             << "  <div class=\"control\"><input type=\"submit\" name=\""<<name<<"\" value=\""<<value<<"\" /></div>\n"
             << "</div>\n";
  }
};

class head {
        String title;
        public:
        void setTitle(const String& str) { title=str; }
        head() : title("Galerie photo") {}
        void doStart(HttpServletRequest& request, HttpServletResponse& response) {
                response
<<"  <head>\n"
<<"<!-- This makes IE6 suck less (a bit) -->\n"
<<"<!--[if lt IE 7]>\n"
<<"<script src=\""<< request.getContextPath() << "/inc/styles/ie7/ie7-standard.js\" type=\"text/javascript\">\n"
<<"</script>\n"
<<"<![endif]-->\n"
<<"    <title>"<<title<<"</title>\n"
<<"    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
<<"    <link rel=\"icon\" href=\""<<request.getContextPath()<<"/stock_camera-16.png\" type=\"image/png\" />\n"
<<"    <link rel=\"shortcut icon\" href=\""<<request.getContextPath()<<"/favicon.ico\" type=\"image/x-icon\" />\n"
<<"    <link type=\"text/css\" rel=\"stylesheet\" href=\""<<request.getContextPath()<<"/inc/styles/dark/dark.css\" title=\"dark\" media=\"screen\" />\n"
<<"    <link type=\"text/css\" rel=\"prefertch alternate stylesheet\" href=\""<<request.getContextPath()<<"/inc/styles/classic/classic.css\" title=\"classic\" media=\"screen\" />\n"
<<"    <link type=\"text/css\" rel=\"prefertch alternate stylesheet\" href=\""<< request.getContextPath()<<"/inc/styles/gorilla/gorilla.css\" title=\"gorilla\" media=\"screen\" />\n"
<<"    <script src=\""<< request.getContextPath()<<"/inc/global.js\" type=\"text/javascript\"></script>\n"
<<"  </head>\n"; 
      	response.setCharacterEncoding("UTF-8");          
                
        }
};
class dropdown {
  String title;
  String desc;
  String name;
  Map<String,String> options;
  String defaultTo;
  public:
  dropdown() : title(""), desc(""), name(""), options(), defaultTo("") {}
  void setTitle(const String& str) { title=str; }
  void setDesc(const String& str) { desc=str; }
  void setName(const String& str) { name=str; }
  void setOptions(const Map<String,String>& opts) { options=opts; }
  void setDefaultTo(const String& str) { defaultTo=str; }
  void doStart(HttpServletRequest& request, HttpServletResponse& response) {
        response << "<div class=\"row\">\n"
                 << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
                 << "  <div class=\"control\"><select name=\""<< name <<"\">\n";

        for ( Map<String,String>::const_iterator it=options.begin();
              it != options.end();
              it++ ) {
                      
                      response << "  <option value=\""<<it->first<<"\" "<< ( (!defaultTo.empty() && it->first==defaultTo)? "selected=\"selected\""  : "" ) <<">"<<it->second<<"</option>\n";
        }
        response << "</select></div>\n"
                  <<"</div>\n";
  }
};
class dirpermdropdown {
        public:
        String title;
        String desc;
        String name;
        dirpermdropdown() : title(""), desc(""), name("") {};
        void setTitle(const String& str) { title=str; }
        void setDesc(const String& str) { desc=str; }
        void setName(const String& str) { name=str; }
        void doStart(HttpServletRequest& request, HttpServletResponse& response) {
                ImageDesc ddesc(path_encode(url_decode(request.getPathInfo())));
                
                bool isPublic=ddesc.isPublic&&!ddesc.isHidden;
                bool isPrivate=!ddesc.isPublic&&!ddesc.isHidden;
                bool isHidden=ddesc.isHidden;
                
                response << "<div class=\"row\">\n"
                        << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
                        << "  <div class=\"control\"><select name=\""<<name<<"\">"
                        << " <option value=\"hidden\" "<<(isHidden?"selected=\"selected\"":"")<<">caché</option>"
                        << " <option value=\"true\" "<<(isPrivate?"selected=\"selected\"":"")<<">privé</option>"
                        << " <option value=\"false\" "<<(isPublic?"selected=\"selected\"":"")<<">public</option>"
                        << " </select></div>\n"
                        << "</div>\n";
        }
};

class dirdatedropdown {
        String title;
        String desc;
        String name;
        String root;
        public:
        void setTitle(const String& str) { title=str; }
        void setDesc(const String& str) { desc=str; }
        void setName(const String& str) { name=str; }
        void setRoot(const String& str) { root=str; }
        dirdatedropdown() : title(""), desc(""), name(""), root("") {}
        void doStart(raii::HttpServletRequest& request, raii::HttpServletResponse& response) {
                const char *mname[]= { NULL, "janvier","février","mars","avril","mai","juin",
                                  "juillet","août","septembre","octobre","novembre","décembre",NULL};

                String pathPart=getVDirPathPart(url_decode(request.getPathInfo()));
                String dirPart=getVDirDirPart(url_decode(request.getPathInfo()));
                int timestamp=0;
                if ( isVDir(url_decode(request.getPathInfo())) ) {
                        Connection conn;
                        String req=String("SELECT date FROM gallery_vdir WHERE path='")+path_encode(pathPart)+"' AND name='"+path_encode(dirPart)+"'";
                        ResultSet rs=conn.query(req);
                        if ( rs.next() ) {
                                timestamp=atoi(rs["date"].c_str());
                        }
                }
                Dir ddesc(root,pathPart,dirPart,timestamp);
                
                time_t now=time(NULL);
                struct tm bdtime;
                if ( !localtime_r(&now,&bdtime) )
                        throw IOException("localtime");
                
                
                response << "<div class=\"row\">\n"
                         << "  <div class=\"label\"><a title=\""<<title<<"\" href=\"#\">"<<desc<<"</a></div>\n"
                            "  <div class=\"control\"><select name=\""<<name<<"_day\">\n";
                for ( int i=1 ; i <= 31 ; ++i ) {
                        response << "    <option value=\""<<i<<"\"" << ( i == ddesc.day ? " selected=\"selected\"" : "" ) << ">"
                                 << i << "</option>\n";
                }
                response << "  </select><select name=\""<<name<<"_month\">\n";
                for ( int i=1 ; i <= 12 ; ++i ) {
                        response << "    <option value=\""<<i<<"\"" << ( i == ddesc.month ? " selected=\"selected\"" : "" ) << ">"
                                 << String(mname[i]) << "</option>\n";
                }
                response << "  </select>""<select name=\""<<name<<"_year\">\n";
                for ( int i=bdtime.tm_year+1900; i > 1900 ; --i ) {
                        response << "    <option value=\""<<i<<"\"" <<  ( i == ddesc.year ? " selected=\"selected\"" : "" ) << ">"
                                 << i << "</option>\n";
                }
                response << "  </select></div>\n"
                          "</div>";

        }
};
        
class footer {
        int sessionCount;
        public:
        footer() : sessionCount(0) {}
        void setSessionCount(int s) { sessionCount=s; } 
        void doStart(HttpServletRequest& request, HttpServletResponse& response) {
                response << "<div class=\"footer\">"
		 "Propulsé par <a href=\"http://blackmilk.fr/www/cms/dev/libapache2_mod_raii\">libapache2-mod-raii</a>, "
		 "<a href=\"http://www.cybercom.net/~dcoffin/dcraw/\">dcraw</a> et "
		 "<a href=\"http://www.imagemagick.org/\">ImageMagick</a>.<br />"
		 "Généré par "
                 "<em><a href=\"http://blackmilk.fr/www/cms/dev/not_so_original\">NotSoOriginal</a> ver. 1.0/rc5</em>.<br />\n"
                 "  Voyeurs connectés : "<< sessionCount << "\n"
                 "</div>\n";
        }
};

class includeSense {

        String filename;
        public:
        void setFilename(const String& str) { filename=str; }
        includeSense() : filename("") {}

        void doStart(raii::HttpServletRequest& request, raii::HttpServletResponse& response) {
        
                if ( ! filename.empty() ) {
                        FILE *f=fopen(filename.c_str(),"r");
                        try {
                                response << f;
                                if (f) fclose(f);
                        }
                        catch ( ... ) {
                                if (f) fclose(f);
                                throw;
                        }
                }

        }
};
