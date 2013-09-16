/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#pragma warning(disable:4786)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <platform.h>
#include "esdlcomp.h"
#include <set>
#include <string>

extern int gOutfile;

//more flexible SCM/ESP code gen initially for RiskWise -- ProtocolX
void ESDLcompiler::write_esp_ng()
{
    //create the *.esp file
    gOutfile = espng;
    outf("// *** Source file generated by ESDL Version %s from %s.scm ***\n", ESDLVER, packagename);
    outf("// *** Not to be hand edited (changes will be lost on re-generation) ***\n\n");

    outf("#ifndef %s_ESP_NG_INCLUDED\n", packagename);
    outf("#define %s_ESP_NG_INCLUDED\n\n", packagename);
    outs("\n");
    outs("#ifdef _WIN32\n");
    outs("#include <process.h>\n");
    outs("#endif\n");

    outs("#include \"bind_ng.hpp\"\n");
    outf("#include \"%s.hpp\"\n", packagename);

    outs("\n\n");

    EspMessageInfo * mi;
    for (mi=msgs;mi;mi=mi->next)
    {
        mi->write_esp_ng_ipp();
    }

    EspServInfo *si;
    for (si=servs;si;si=si->next)
    {
        si->write_esp_binding_ng_ipp(msgs);
        outs("\n\n");
        si->write_esp_service();
        outs("\n\n");
    }

    outf("#endif //%s_ESP_NG_INCLUDED\n", packagename);
}

void ESDLcompiler::write_esp_ng_cpp()
{
    //create the *.esp file
    gOutfile = espngc;
    outf("// *** Source file generated by ESDL Version %s from %s.scm ***\n", ESDLVER, packagename);
    outf("// *** Not to be hand edited (changes will be lost on re-generation) ***\n\n");
    outf("#include \"%s_esp_ng.ipp\"\n\n\n", packagename);

    EspServInfo *si;
    for (si=servs;si;si=si->next)
    {
        si->write_esp_binding_ng_cpp(msgs);
        outs("\n\n");
    }
}

void EspMessageInfo::write_esp_ng_ipp()
{
    const char *parent=getParentName();
    const char *baseclass=parent;
    if (!baseclass)
    {
        switch(espm_type_)
        {
            case espm_struct:
                baseclass="CEspNgStruct";
                break;
            case espm_request:
                baseclass="CEspNgRequest";
                break;
            default:
                baseclass="CEspNgResponse";
                break;
        }
    }
    outf("class CNg%s : public %s,\n", name_, baseclass);

    outf("   implements IEsp%s\n", name_);
    outs("{\n");

    outs("protected:\n");
    int pcount=0;
    ParamInfo *pi=NULL;
    for (pi=getParams();pi!=NULL;pi=pi->next)
        pcount++;

    outs(1, "int m_pcount;\n");
    outf(1, "IEspNgParameter *m_params[%d];\n", pcount);

    outs("public:\n\n");

    outs(1,"StringBuffer m_serviceName;\n");
    outs(1,"StringBuffer m_methodName;\n");
    outs(1,"StringBuffer m_msgName;\n");

    outs(1,"IMPLEMENT_IINTERFACE;\n\n");

    outs(1, "void init_params()\n");
    outs(1, "{\n");
    outf(2, "m_pcount=%d;\n", pcount);
    int pidx=0;
    for (pi=getParams();pi!=NULL;pi=pi->next)
    {
        pi->write_esp_ng_declaration(pidx++);
    }
    outs(1, "}\n\n");

    outs(1, "virtual IEspNgParameterIterator * getParameterIterator()\n");
    outs(1, "{\n");
    outs(2, "return new EspNgParameterIterator(m_params, m_pcount);\n");
    outs(1, "}\n");

    //default constructor
    outf(1, "CNg%s(const char *serviceName){\n", name_);
    outs(2, "m_serviceName.append(serviceName);\n");
    outs(2, "init_params();\n");
    outs(1, "}\n");

    //default destructor -- bhegerle
    outf(1, "~CNg%s() {\n", name_);
    outs(2, "for(int i=0; i < m_pcount; i++)\n");
    outs(3, "m_params[i]->Release();\n");
    outs(1, "}\n");

    if (espm_type_==espm_response)
    {
        outs(1,"virtual void setRedirectUrl(const char *url){CEspNgResponse::setRedirectUrl(url);}\n");
        outs(1,"virtual const IMultiException & getExceptions(){return CEspNgResponse::getExceptions();}\n");
        outs(1,"virtual void noteException(IException & e){CEspNgResponse::noteException(e);}\n");
    }

    outs("\n");
    write_esp_methods_ng(espaxm_both);

    outf(1, "virtual void copy(IConst%s &from){/*not implemented*/}\n", name_);
    outf(1, "virtual const char * queryName(){return \"%s\";}\n", name_);

    outs("};\n\n");
}


void ParamInfo::write_esp_ng_declaration(int pos)
{
    char metatype[256]={0};
    cat_type(metatype);

    esp_xlate_info *xlation=esp_xlat(metatype, false);
    const char *type_name="StringBuffer";
    if (xlation)
    {
        if (xlation->eam_type==EAM_jmbin || (flags & PF_TEMPLATE) || getMetaInt("attach", 0))
            return; //not implemented
        type_name=xlation->store_type;
    }

    int maxlen=getMetaInt("maxlen", -1);
    if (!strcmp(type_name, "StringBuffer"))
        outf(2, "m_params[%d]=new EspNgStringParameter(\"%s\", %d);\n", pos, name, maxlen);
    else
        outf(2, "m_params[%d]=new EspNgParameter<%s>(\"%s\", %d);\n", pos, type_name, name, maxlen);
}

void ParamInfo::write_esp_attr_method_ng(const char *msgname, int pos, bool isSet, bool hasNilRemove)
{
    char metatype[256]={0};
    cat_type(metatype);

    esp_xlate_info *xlation=esp_xlat(metatype);

    char *methName=strdup(name);
    *methName=upperchar(*methName);

    if (isSet)
    {
        if (hasNilRemove)
            outf(1,"void set%s_null(){ m_params[%d]->setNull(); }\n", methName, pos);
        outf(1,"void set%s(%s val){m_params[%d]->setValue(val);}\n", methName, xlation->access_type, pos);
    }
    else
    {
        if (hasNilRemove)
            outf(1,"bool get%s_isNull(){return m_params[%d]->isNull();}\n", methName, pos);
        outf(1,"%s get%s(){return EspNgParamConverter(m_params[%d]);}\n", xlation->access_type, methName, pos);
    }
    free(methName);
}

void EspMessageInfo::write_esp_methods_ng(enum espaxm_type axstype)
{
    if (axstype!=espaxm_setters)
    {
        int pos=0;
        ParamInfo *pi;
        for (pi=getParams();pi!=NULL;pi=pi->next)
        {
            pi->write_esp_attr_method_ng(name_, pos++, false, getMetaInt("nil_remove")!=0);
        }
    }

    if (axstype!=espaxm_getters)
    {
        int pos=0;
        ParamInfo *pi;
        for (pi=getParams();pi!=NULL;pi=pi->next)
        {
            pi->write_esp_attr_method_ng(name_, pos++, true, getMetaInt("nil_remove")!=0);
        }
    }
}


void EspServInfo::write_esp_binding_ng_ipp(EspMessageInfo *msgs)
{
    EspMethodInfo *mthi=NULL;

    outs("\n\n");
    outf("template <class base_binding> class CNg%sServiceBinding : public base_binding\n", name_);
    outs("{\npublic:\n");

    outf(1,"CNg%sServiceBinding(IPropertyTree* cfg, const char *bindname=NULL, const char *procname=NULL) : base_binding(cfg, bindname, procname){}\n", name_);

    outs(1,"IEspNgRequest* createRequest(const char *method)\n");
    outs(1,"{\n");
    int count=0;
    for (mthi=methods;mthi;mthi=mthi->next)
    {
        outf(2,"%sif (!stricmp(method, \"%s\"))\n", (count++==0)? "" : "else ", mthi->getName());
        outs(2,"{\n");
        outf(3,"return new CNg%s(\"%s\");\n", mthi->getReq(), name_);
        outs(2,"}\n");
    }
    outs(2,"return NULL;\n");
    outs(1,"}\n\n");

    outs(1,"virtual IEspNgResponse* createResponse(const char *method)\n");
    outs(1,"{\n");
    count=0;
    for (mthi=methods;mthi;mthi=mthi->next)
    {
        outf(2,"%sif (!stricmp(method, \"%s\"))\n", (count++==0)? "" : "else ", mthi->getName());
        outs(2,"{\n");
        outf(3,"return new CNg%s(\"%s\");\n", mthi->getResp(), name_);
        outs(2,"}\n");
    }
    outs(2,"return NULL;\n");
    outs(1,"}\n\n");

    //method ==> processRequest
    outs(1,"virtual int processRequest(IEspContext &context, const char *method_name, IEspNgRequest* req, IEspNgResponse* resp)\n");
    outs(1,"{\n");
    outf(2, "Owned<IEsp%s> iserv = (IEsp%s*)base_binding::getService();\n", name_, name_);
    count=0;
    for (mthi=methods;mthi;mthi=mthi->next)
    {
        outf(2,"%sif (!stricmp(method_name, \"%s\")/*||!stricmp(req->queryName(), \"%s\")*/)\n", (count++==0)? "" : "else ", mthi->getName(), mthi->getReq());
        outs(2,"{\n");
        outf(3,"return iserv->on%s(context, *dynamic_cast<IEsp%s*>(req), *dynamic_cast<IEsp%s*>(resp));\n", mthi->getName(), mthi->getReq(), mthi->getResp());
        outs(2,"}\n");
    }
    outs(2,"return 0;\n");
    outs(1,"}\n\n");

    //method ==> getServiceName
    outf(1,"StringBuffer & getServiceName(StringBuffer &resp){resp.clear().append(\"%s\");}\n", name_);

    outs("};\n\n");
}

void EspServInfo::write_esp_binding_ng_cpp(EspMessageInfo *msgs)
{
}