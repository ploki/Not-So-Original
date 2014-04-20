#include "raii.H"
#include <Gallery/common.H>
#include <Magick++.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>


using namespace Magick;

using namespace raii;

class SERVLET(edit) : public HttpServlet {


        void service ( HttpServletRequest& request, HttpServletResponse& response) {

                securityCheck(this,request,response);

                String pathpart=url_decode(request.getPathInfo()).substr(0,request.getPathInfo().rfind("/")) + "/";
                String namepart=url_decode(request.getPathInfo()).substr(request.getPathInfo().rfind("/")+1);

                String action=request.getParameter("action");
                ptr<HttpSession> session=request.getSession();

                if ( action.empty() ) {
                    request.getRequestDispatcher("/edit.csp").forward(request,response);
                }
                else if ( action == "edit" ) {
                    if ( ! session->getAttribute(request.getPathInfo()+"/process") ) {
                        ptr<Process> newProcess = new Process(getInitParameter("ROOT"),getInitParameter("TMP"),url_decode(request.getPathInfo()));
                        Connection conn;
                        if ( isRaw(getInitParameter("ROOT")+url_decode(request.getPathInfo() ) ) ) newProcess->loadFrom(conn,"__default__");
                        newProcess->loadFrom(conn);
                        session->setAttribute(request.getPathInfo()+"/process", newProcess);
                    }
                    response.sendRedirect(response.encodeRedirectURL(request.getContextPath()+"/edit.C"+request.getPathInfo()));
                }
                else if ( action == "add_operator" ) {
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                    String op = request.getParameter("operator");
                    //reste à faire
                    //crop/resize dans load
                    //zone_mapper
                    //gamma
                    //levels
                    //shadowhighlights
                    process->addOperator(op);
                    request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);
                }
                else if ( action == "delete_operator" ) {
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                    int opId = request.getParameter("operator").toi();
                    process->deleteOperator(opId);
                    request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);
                }
                else if ( action == "toggle_enabled" ) {
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                    int opId = request.getParameter("operator").toi();
                    ptr<op::Operator> ope = process->getOperator(opId);
                    if ( ope->isEnabled() ) ope->disable(); else ope->enable();
                    request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);
                }
                else if ( action == "setUpTo" ) {
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");
                    process->upTo=request.getParameter("operator").toi();
                    request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);
                }
                else if ( action == "show_process" ) {
                        //renvoie l'image résultat du process (copie de travail)
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                        Magick::Blob blob;
                        process->getBlob(720,&blob);
                        response.setContentType("image/jpg");
                        if ( ap_rwrite(blob.data(),blob.length(),apacheRequest) < 0 )
                                throw ResponseException("Connexion closed");

                }
                else if ( action == "show_fullprocess" ) {
                        //renvoie l'image résultat du process (grande taille)
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                        Magick::Blob blob;
                        process->getBlob(0,&blob);
                        response.setContentType("image/jpg");
                        if ( ap_rwrite(blob.data(),blob.length(),apacheRequest) < 0 )
                                throw ResponseException("Connexion closed");

                }
                else if ( action == "silent_mod" ) {
                        //Ajax, changement d'un attribut d'un opérateur
                    ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                    if ( ! process ) throw ServletException("No process defined");

                    ptr<op::Operator> op = process->getOperator(request.getParameter("operator").toi());
                    op->mod(request.getParameter("name"),
                            request.getParameter("value"));
                    request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);
                }
                else if ( action == "process" ) {
                     String control = request.getParameter("control");
                     if ( control == "reset" ) {
                     }
                     else if ( control == "save" ) {
                        //enregistre le processus dans la base de donnée et invalide les thumbnail/preview/processed
                        ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                        Connection conn;
                        process->saveTo(conn);
                     }
                     else if ( control == "setdefault" ) {
                        //enregistre le processus dans la base de donnée comme traitement par défaut
                        ptr<Process> process = session->getAttribute(request.getPathInfo()+"/process");
                        Connection conn;
                        process->saveTo(conn,"__default__");
               }
                     else if ( control == "quit" ) {
                        //purge la session
                        session->setAttribute(request.getPathInfo()+"/process",NULL);
                        response.sendRedirect(response.encodeRedirectURL(request.getContextPath()+"/gallery.C"+request.getPathInfo()+"?action=show"));
                     }
                     else
                        throw ServletException("Unknown process control");
                     request.getRequestDispatcher("/operator/_stack.csp").forward(request,response);

               }
                else
                        throw ServletException("Unknown action");
        }

};
exportServlet(edit);
