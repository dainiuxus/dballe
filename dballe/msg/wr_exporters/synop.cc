/*
 * Copyright (C) 2005--2011  ARPA-SIM <urpsim@smr.arpa.emr.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author: Enrico Zini <enrico@enricozini.com>
 */

#include "msg/wr_codec.h"
#include <wreport/bulletin.h>
#include "msg/msgs.h"
#include "msg/context.h"

#warning TODO: remove when done
#include <iostream>

using namespace wreport;
using namespace std;

#define SYNOP_NAME "synop"
#define SYNOP_DESC "Synop (autodetect)"

#define SYNOP_ECMWF_NAME "synop-ecmwf"
#define SYNOP_ECMWF_DESC "Synop ECMWF (autodetect) (0.1)"

#define SYNOP_WMO_NAME "synop-wmo"
#define SYNOP_WMO_DESC "Synop WMO (0.1)"

#define SYNOP_ECMWF_LAND_NAME "synop-ecmwf-land"
#define SYNOP_ECMWF_LAND_DESC "Synop ECMWF land (0.1)"

#define SYNOP_ECMWF_LAND_HIGH_NAME "synop-ecmwf-land-high"
#define SYNOP_ECMWF_LAND_HIGH_DESC "Synop ECMWF land high level station (0.1)"

#define SYNOP_ECMWF_AUTO_NAME "synop-ecmwf-auto"
#define SYNOP_ECMWF_AUTO_DESC "Synop ECMWF land auto (0.3)"

namespace dballe {
namespace msg {
namespace wr {

namespace {

static const Trange tr_std_past_wtr3(205, 0, 10800);
static const Trange tr_std_past_wtr6(205, 0, 21600);

// If var is not NULL and has a B04194 attribute, return its value
// otherwise, return orig
static int override_trange(const Var* var, int orig)
{
    if (var)
        if (const Var* a = var->enqa(WR_VAR(0, 4, 194)))
            return a->enq(orig);
    return orig;
}

struct ContextFinder
{
    struct Entry
    {
        Varcode code;
        const Var* var;
        Entry(Varcode code) : code(code), var(0) {}
        void add(Subset& subset)
        {
            if (var)
                subset.store_variable(*var);
            else
                subset.store_variable_undef(code);
        }
        void add(Subset& subset, Varcode as_code)
        {
            if (var)
                subset.store_variable(as_code, *var);
            else
                subset.store_variable_undef(as_code);
        }
    };

    const Msg& msg;
    const msg::Context* ctx;
    vector<Entry> vars;

    ContextFinder(const Msg& msg) : msg(msg), ctx(0)
    {
    }

    void add_var(Varcode code)
    {
        vars.push_back(Entry(code));
    }
    void add_var(int shortcut)
    {
        add_var(shortcutTable[shortcut].code);
    }

    bool scan_vars_in_context(const msg::Context& c)
    {
        bool found = false;
        for (vector<Entry>::iterator i = vars.begin();
                i != vars.end(); ++i)
        {
            if (const Var* var = c.find(i->code))
            {
                found = true;
                i->var = var;
            }
        }
        return found;
    }

    bool find_in_level(int ltype1, int or_shortcut=-1)
    {
        bool found = false;
        if (or_shortcut != -1)
        {
            if (const msg::Context* c = msg.find_context_by_id(or_shortcut))
                if (scan_vars_in_context(*c))
                {
                    ctx = c;
                    found = true;
                }
        }
        for (std::vector<msg::Context*>::const_iterator i = msg.data.begin();
                !found && i != msg.data.end(); ++i)
        {
            const msg::Context* c = *i;
            if (c->level.ltype1 != ltype1) continue;
            if (scan_vars_in_context(*c))
            {
                ctx = c;
                found = true;
            }
        }
        return found;
    }

    const Var* find_first_attr(Varcode code) const
    {
        for (vector<Entry>::const_iterator i = vars.begin();
                i != vars.end(); ++i)
        {
            if (!i->var) continue;
            if (const Var* a = i->var->enqa(code))
                return a;
        }
        return NULL;
    }

    void add_found_sensor_height(Subset& subset)
    {
        if (!ctx)
        {
            subset.store_variable_undef(WR_VAR(0, 7, 32));
            return;
        }

        // Check the attributes to see if we're exporting a message
        // imported with the 'simplified' method
        if (const Var* a = find_first_attr(WR_VAR(0, 7, 32)))
            subset.store_variable_d(WR_VAR(0, 7, 32), a->enqd());
        else if (ctx->level.ltype1 == 1)
            // Ground level
            subset.store_variable_d(WR_VAR(0, 7, 32), 0);
        else if (ctx->level.ltype1 == 103)
            // Height above ground level
            subset.store_variable_d(WR_VAR(0, 7, 32), double(ctx->level.l1) / 1000.0);
        else
            error_consistency::throwf("Cannot add sensor height from unsupported level type %d", ctx->level.ltype1);
    }
};

struct PressureInfo
{
    const Var* v_press;
    const Var* v_pressmsl;
    const Var* v_pchange3;
    const Var* v_pchange24;
    const Var* v_ptend;

    void reset()
    {
        v_press = 0;
        v_pressmsl = 0;
        v_pchange3 = 0;
        v_pchange24 = 0;
        v_ptend = 0;
    }

    void scan_context4(const msg::Context& c)
    {
        if (const Var* v = c.find_by_id(DBA_MSG_PRESS_3H))
            switch (c.trange.p2)
            {
                case  3*3600: v_pchange3 = v; break;
                case 24*3600: v_pchange24 = v; break;
            }
    }
    void scan_context205(const msg::Context& c)
    {
        if (const Var* v = c.find_by_id(DBA_MSG_PRESS_TEND))
            v_ptend = v;
    }
    void scan_context254(const msg::Context& c)
    {
        if (const Var* v = c.find_by_id(DBA_MSG_PRESS))
            v_press = v;
        if (const Var* v = c.find_by_id(DBA_MSG_PRESS_MSL))
            v_pressmsl = v;
    }
    void scan_context(const msg::Context& c)
    {
        switch (c.trange.pind)
        {
            case 4: scan_context4(c); break;
            case 205: scan_context205(c); break;
            case 254: scan_context254(c); break;
        }
    }
};

struct PrecInfo
{
    const msg::Context* c_prec1;
    const msg::Context* c_prec2;
    const msg::Context* c_prec24;

    void reset()
    {
        c_prec1 = NULL;
        c_prec2 = NULL;
        c_prec24 = NULL;
    }

    void scan_context1(const msg::Context& c)
    {
        if (c.find(WR_VAR(0, 13, 11)))
        {
            if (c.trange.p2 == 86400)
                c_prec24 = &c;
            else if (!c_prec1)
                c_prec1 = &c;
            else if (!c_prec2)
                c_prec2 = &c;
        }
    }
};

struct Synop : public Template
{
    const msg::Context* c_sunshine1;
    const msg::Context* c_sunshine2;
    const msg::Context* c_wind;
    const msg::Context* c_gust1;
    const msg::Context* c_gust2;
    const msg::Context* c_evapo;
    const msg::Context* c_past_wtr;
    const msg::Context* c_thermo;
    const msg::Context* c_visib;
    const msg::Context* c_tmax;
    const msg::Context* c_tmin;
    PressureInfo i_press;
    PrecInfo i_prec;

    Synop(const Exporter::Options& opts, const Msgs& msgs)
        : Template(opts, msgs) {}

    void scan_levels()
    {
        c_sunshine1 = NULL;
        c_sunshine2 = NULL;
        c_wind = NULL;
        c_gust1 = NULL;
        c_gust2 = NULL;
        c_evapo = NULL;
        c_past_wtr = NULL;
        c_thermo = NULL;
        c_visib = NULL;
        c_tmax = NULL;
        c_tmin = NULL;
        i_press.reset();
        i_prec.reset();

        // Scan message finding context for the data that follow
        for (std::vector<msg::Context*>::const_iterator i = msg->data.begin();
                i != msg->data.end(); ++i)
        {
            const msg::Context* c = *i;
            switch (c->level.ltype1)
            {
                case 1:
                    switch (c->trange.pind)
                    {
                        case 1:
                            // msg->set(var, WR_VAR(0, 14, 31), Level(1), Trange(1, 0, time_period));
                            if (c->find(WR_VAR(0, 14, 31)))
                            {
                                if (!c_sunshine1)
                                    c_sunshine1 = c;
                                else if (!c_sunshine2)
                                    c_sunshine2 = c;
                            }

                            i_prec.scan_context1(*c);

                            // msg->set(var, WR_VAR(0, 13, 33), Level(1), Trange(1, 0, -time_period));
                            if (c->find(WR_VAR(0, 13, 33)))
                                c_evapo = c;
                            break;
                        case 2:
                            if (c->find(WR_VAR(0, 12, 101))) c_tmax = c;
                            break;
                        case 3:
                            if (c->find(WR_VAR(0, 12, 101))) c_tmin = c;
                            break;
                        case 4: i_press.scan_context4(*c); break;
                        case 205:
                            if (c->find(WR_VAR(0, 20, 4)) || c->find(WR_VAR(0, 20, 5)))
                                c_past_wtr = c;
                            i_press.scan_context205(*c);
                            break;
                        case 254:
                            if (c->find_by_id(DBA_MSG_VISIBILITY))
                                c_visib = c;
                            i_press.scan_context254(*c);
                            break;
                    }
                    break;
                case 101:
                case 102:
                    i_press.scan_context(*c);
                    break;
                case 103:
                    switch (c->trange.pind)
                    {
                        case 1: i_prec.scan_context1(*c); break;
                        case 2:
                            if (c->find(WR_VAR(0, 12, 101))) c_tmax = c;
                            break;
                        case 3:
                            if (c->find(WR_VAR(0, 12, 101))) c_tmin = c;
                            break;
                        case 254:
                            if (c->find_by_id(DBA_MSG_TEMP_2M) || c->find_by_id(DBA_MSG_DEWPOINT_2M) || c->find_by_id(DBA_MSG_HUMIDITY))
                                c_thermo = c;
                            break;
                    }
                    if (c->find(WR_VAR(0, 11, 1)) || c->find(WR_VAR(0, 11, 2)))
                        if (!c_wind)
                            c_wind = c;
                    if (c->find(WR_VAR(0, 11, 41)) || c->find(WR_VAR(0, 11, 43)))
                    {
                        if (!c_gust1)
                            c_gust1 = c;
                        else if (!c_gust2)
                            c_gust2 = c;
                    }
                    if (c->find_by_id(DBA_MSG_VISIBILITY))
                        c_visib = c;
                    break;
            }
        }
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Template::to_subset(msg, subset);
        scan_levels();
    }
};

// Base template for synops
struct SynopECMWF : public Synop
{
    bool is_crex;
    Varcode prec_code;

    SynopECMWF(const Exporter::Options& opts, const Msgs& msgs)
        : Synop(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_DESC; }

    void add_prec()
    {
        const Var* var = NULL;
        switch (prec_code)
        {
            case WR_VAR(0, 13, 23): var = msg->get_tot_prec24_var(); break;
            case WR_VAR(0, 13, 22): var = msg->get_tot_prec12_var(); break;
            case WR_VAR(0, 13, 21): var = msg->get_tot_prec6_var(); break;
            case WR_VAR(0, 13, 20): var = msg->get_tot_prec3_var(); break;
            case WR_VAR(0, 13, 19): var = msg->get_tot_prec1_var(); break;
        }
        if (var)
            subset->store_variable(prec_code, *var);
        else
            subset->store_variable_undef(prec_code);
    }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        Synop::setupBulletin(bulletin);

        // Use old table for old templates
        if (BufrBulletin* b = dynamic_cast<BufrBulletin*>(&bulletin))
        {
            b->master_table = 13;
        }

        is_crex = dynamic_cast<CrexBulletin*>(&bulletin) != 0;

        // Use the best kind of precipitation found in the message to encode
        prec_code = 0;
        for (Msgs::const_iterator mi = msgs.begin(); prec_code == 0 && mi != msgs.end(); ++mi)
        {
            const Msg& msg = **mi;
            if (msg.get_tot_prec24_var() != NULL)
                prec_code = WR_VAR(0, 13, 23);
            else if (msg.get_tot_prec12_var() != NULL)
                prec_code = WR_VAR(0, 13, 22);
            else if (msg.get_tot_prec6_var() != NULL)
                prec_code = WR_VAR(0, 13, 21);
            else if (msg.get_tot_prec3_var() != NULL)
                prec_code = WR_VAR(0, 13, 20);
            else if (msg.get_tot_prec1_var() != NULL)
                prec_code = WR_VAR(0, 13, 19);
        }
        if (prec_code == 0)
            prec_code = WR_VAR(0, 13, 23);

        bulletin.type = 0;
        bulletin.subtype = 255;
        bulletin.localsubtype = 1;
    }
    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Synop::to_subset(msg, subset);
        /*  0 */ add(WR_VAR(0,  1,  1), DBA_MSG_BLOCK);
        /*  1 */ add(WR_VAR(0,  1,  2), DBA_MSG_STATION);
        /*  2 */ add(WR_VAR(0,  2,  1), DBA_MSG_ST_TYPE);
        /*  3 */ add(WR_VAR(0,  4,  1), DBA_MSG_YEAR);
        /*  4 */ add(WR_VAR(0,  4,  2), DBA_MSG_MONTH);
        /*  5 */ add(WR_VAR(0,  4,  3), DBA_MSG_DAY);
        /*  6 */ add(WR_VAR(0,  4,  4), DBA_MSG_HOUR);
        /*  7 */ add(WR_VAR(0,  4,  5), DBA_MSG_MINUTE);
        /*  8 */ add(WR_VAR(0,  5,  1), DBA_MSG_LATITUDE);
        /*  9 */ add(WR_VAR(0,  6,  1), DBA_MSG_LONGITUDE);
        /* 10 */ add(WR_VAR(0,  7,  1), DBA_MSG_HEIGHT_STATION);
        /* 11 */ add(WR_VAR(0, 10,  4), i_press.v_press);
    }
};

struct SynopECMWFLand : public SynopECMWF
{
    SynopECMWFLand(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWF(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_LAND_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_LAND_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWF::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  7,  5));
        bulletin.datadesc.push_back(prec_code);
        bulletin.datadesc.push_back(WR_VAR(0, 13, 13));
        if (!is_crex)
        {
            bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 49));
            bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 49));
            bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        SynopECMWF::to_subset(msg, subset);
        /* 12 */ add(WR_VAR(0, 10, 51), i_press.v_pressmsl);
        /* 13 */ add(WR_VAR(0, 10, 61), i_press.v_pchange3);
        /* 14 */ add(WR_VAR(0, 10, 63), i_press.v_ptend);
        /* 15 */ add(WR_VAR(0, 11, 11), c_wind, DBA_MSG_WIND_DIR);
        /* 16 */ add(WR_VAR(0, 11, 12), c_wind, DBA_MSG_WIND_SPEED);
        /* 17 */ add(WR_VAR(0, 12,  4), DBA_MSG_TEMP_2M);
        /* 18 */ add(WR_VAR(0, 12,  6), DBA_MSG_DEWPOINT_2M);
        /* 19 */ add(WR_VAR(0, 13,  3), DBA_MSG_HUMIDITY);
        /* 20 */ add(WR_VAR(0, 20,  1), DBA_MSG_VISIBILITY);
        /* 21 */ add(WR_VAR(0, 20,  3), DBA_MSG_PRES_WTR);
        do_ecmwf_past_wtr();
        /* 24 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 25 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 26 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 27 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 28 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 29 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 30 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        /* 31 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 1), Trange::instant());
        /* 32 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N1);
        /* 33 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C1);
        /* 34 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H1);
        /* 35 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 2), Trange::instant());
        /* 36 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N2);
        /* 37 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C2);
        /* 38 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H2);
        /* 39 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 3), Trange::instant());
        /* 40 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N3);
        /* 41 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C3);
        /* 42 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H3);
        /* 43 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 4), Trange::instant());
        /* 44 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N4);
        /* 45 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C4);
        /* 46 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H4);
        /* 47 */ add_prec();
        /* 48 */ add(WR_VAR(0, 13, 13), DBA_MSG_TOT_SNOW);

        if (!is_crex)
        {
            subset.append_fixed_dpb(WR_VAR(2, 22, 0), 49);
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
        }
    }
};

struct SynopECMWFLandHigh : public SynopECMWF
{
    SynopECMWFLandHigh(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWF(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_LAND_HIGH_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_LAND_HIGH_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWF::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  7,  7));
        bulletin.datadesc.push_back(prec_code);
        bulletin.datadesc.push_back(WR_VAR(0, 13, 13));
        if (!is_crex)
        {
            bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 34));
            bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 34));
            bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        SynopECMWF::to_subset(msg, subset);

        ///* 12 */ add(WR_VAR(0,  7,  4), DBA_MSG_ISOBARIC_SURFACE);
        ///* 13 */ add(WR_VAR(0, 10,  3), DBA_MSG_GEOPOTENTIAL);
        // Find pressure level of geopotential
        ContextFinder finder(msg);
        finder.add_var(WR_VAR(0, 10, 8));
        if (finder.find_in_level(100))
            subset.store_variable_d(WR_VAR(0, 7, 4), finder.ctx->level.l1);
        else
            subset.store_variable_undef(WR_VAR(0,  7, 4));
        finder.vars[0].add(subset, WR_VAR(0, 10, 3));

        /* 14 */ add(WR_VAR(0, 10, 61), i_press.v_pchange3);
        /* 15 */ add(WR_VAR(0, 10, 63), i_press.v_ptend);
        /* 16 */ add(WR_VAR(0, 11, 11), c_wind, DBA_MSG_WIND_DIR);
        /* 17 */ add(WR_VAR(0, 11, 12), c_wind, DBA_MSG_WIND_SPEED);
        /* 18 */ add(WR_VAR(0, 12,  4), DBA_MSG_TEMP_2M);
        /* 19 */ add(WR_VAR(0, 12,  6), DBA_MSG_DEWPOINT_2M);
        /* 20 */ add(WR_VAR(0, 13,  3), DBA_MSG_HUMIDITY);
        /* 21 */ add(WR_VAR(0, 20,  1), DBA_MSG_VISIBILITY);
        /* 22 */ add(WR_VAR(0, 20,  3), DBA_MSG_PRES_WTR);
        do_ecmwf_past_wtr();
        /* 25 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 26 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 27 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 28 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 29 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 30 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 31 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        /* 32 */ add_prec();
        /* 33 */ add(WR_VAR(0, 13, 13), DBA_MSG_TOT_SNOW);

        if (!is_crex)
        {
            subset.append_fixed_dpb(WR_VAR(2, 22, 0), 34);
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
        }
    }
};

// Same as SynopECMWFLandHigh but just with a different local subtype
struct SynopECMWFAuto : public SynopECMWFLand
{
    SynopECMWFAuto(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWFLand(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_AUTO_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_AUTO_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWFLand::setupBulletin(bulletin);

        bulletin.localsubtype = 3;
    }
};

struct SynopWMO : public Synop
{
    bool is_crex;

    SynopWMO(const Exporter::Options& opts, const Msgs& msgs)
        : Synop(opts, msgs) {}

    virtual const char* name() const { return SYNOP_WMO_NAME; }
    virtual const char* description() const { return SYNOP_WMO_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        Synop::setupBulletin(bulletin);

        is_crex = dynamic_cast<CrexBulletin*>(&bulletin) != 0;

        bulletin.type = 0;
        bulletin.subtype = 255;
        bulletin.localsubtype = 1;

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3, 7, 80));
        bulletin.load_tables();
    }

    // D02031  Pressure data
    void do_D02031(const Msg& msg, wreport::Subset& subset)
    {
        add(WR_VAR(0, 10,  4), i_press.v_press);
        add(WR_VAR(0, 10, 51), i_press.v_pressmsl);
        add(WR_VAR(0, 10, 61), i_press.v_pchange3);
        add(WR_VAR(0, 10, 63), i_press.v_ptend);
        add(WR_VAR(0, 10, 62), i_press.v_pchange24);

        // Find pressure level of geopotential
        ContextFinder finder(msg);
        finder.add_var(WR_VAR(0, 10, 8));
        if (finder.find_in_level(100))
            subset.store_variable_d(WR_VAR(0, 7, 4), finder.ctx->level.l1);
        else
            subset.store_variable_undef(WR_VAR(0,  7, 4));
        finder.vars[0].add(subset, WR_VAR(0, 10, 9));
    }

    void add_sensor_height(const msg::Context& c, const Var* sample_var=NULL)
    {
        // Try with attributes first
        if (sample_var)
        {
            if (const Var* a = sample_var->enqa(WR_VAR(0, 7, 32)))
            {
                subset->store_variable_d(WR_VAR(0, 7, 32), a->enqd());
                return;
            }
        }

        // Use level
        if (c.level.ltype1 == 1)
            // Ground level
            subset->store_variable_d(WR_VAR(0, 7, 32), 0);
        else if (c.level.ltype1 == 103)
        {
            // Height above ground level
            if (c.level.l1 == MISSING_INT)
                subset->store_variable_undef(WR_VAR(0, 7, 32));
            else
                subset->store_variable_d(WR_VAR(0, 7, 32), double(c.level.l1) / 1000.0);
        }
        else
            error_consistency::throwf("Cannot add sensor height from unsupported level type %d", c.level.ltype1);
    }

    // D02035  Basis synoptic "instantaneous" data
    void do_D02035(const Msg& msg, wreport::Subset& subset)
    {
        //   Temperature and humidity data
        {
            if (c_thermo)
            {
                const Var* var = c_thermo->find_by_id(DBA_MSG_TEMP_2M);
                if (!var) var = c_thermo->find_by_id(DBA_MSG_DEWPOINT_2M);
                if (!var) var = c_thermo->find_by_id(DBA_MSG_HUMIDITY);
                add_sensor_height(*c_thermo, var);
            } else
                subset.store_variable_undef(WR_VAR(0, 7, 32));
            add(WR_VAR(0, 12, 101), c_thermo, DBA_MSG_TEMP_2M);
            add(WR_VAR(0, 12, 103), c_thermo, DBA_MSG_DEWPOINT_2M);
            add(WR_VAR(0, 13,   3), c_thermo, DBA_MSG_HUMIDITY);
        }

        //   Visibility data
        {
            if (c_visib)
            {
                const Var* var = c_visib->find_by_id(DBA_MSG_VISIBILITY);
                add_sensor_height(*c_visib, var);
            } else
                subset.store_variable_undef(WR_VAR(0, 7, 32));
            add(WR_VAR(0, 20, 1), c_visib, DBA_MSG_VISIBILITY);
        }

        //   Precipitation past 24 hours
        if (i_prec.c_prec24)
        {
            const Var* var = i_prec.c_prec24->find(WR_VAR(0, 13, 11));
            add_sensor_height(*i_prec.c_prec24, var);
            add(WR_VAR(0, 13, 23), i_prec.c_prec24, DBA_MSG_TOT_PREC24);
        } else {
            subset.store_variable_undef(WR_VAR(0,  7, 32));
            subset.store_variable_undef(WR_VAR(0, 13, 23));
        }
        subset.store_variable_undef(WR_VAR(0, 7, 32));

        //   Cloud data
        {
            add(WR_VAR(0, 20,  10), DBA_MSG_CLOUD_N);
            add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
            add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
            add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
            add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
            add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
            add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);

            //   Individual cloud layers or masses
            int max_cloud_group = 0;
            for (int i = 1; ; ++i)
            {
                if (msg.find_context(Level::cloud(259, i), Trange::instant()))
                {
                    max_cloud_group = i;
                } else if (i > 4)
                    break;
            }

            // Number of individual cloud layers or masses
            subset.store_variable_i(WR_VAR(0, 1, 31), max_cloud_group);
            for (int i = 1; i <= max_cloud_group; ++i)
            {
                add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, i), Trange::instant());
                if (const msg::Context* c = msg.find_context(Level::cloud(259, i), Trange::instant()))
                {
                    add(WR_VAR(0, 20, 11), c, DBA_MSG_CLOUD_N1);
                    add(WR_VAR(0, 20, 12), c, DBA_MSG_CLOUD_C1);
                    add(WR_VAR(0, 20, 13), c, DBA_MSG_CLOUD_H1);
                } else {
                    subset.store_variable_undef(WR_VAR(0, 20, 11));
                    subset.store_variable_undef(WR_VAR(0, 20, 12));
                    subset.store_variable_undef(WR_VAR(0, 20, 13));
                }
            }
        }
    }

    // D02036  Clouds with bases below station level
    void do_D02036(const Msg& msg, wreport::Subset& subset)
    {
        // Number of individual cloud layers or masses
        int max_cloud_group = 0;
        for (int i = 1; ; ++i)
        {
            if (msg.find_context(Level::cloud(263, i), Trange::instant()))
            {
                max_cloud_group = i;
            } else if (i > 4)
                break;
        }
        subset.store_variable_i(WR_VAR(0, 1, 31), max_cloud_group);
        for (int i = 1; i <= max_cloud_group; ++i)
        {
            add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(263, i), Trange::instant());
            const msg::Context* c = msg.find_context(Level::cloud(263, i), Trange::instant());
            add(WR_VAR(0, 20, 11), c);
            add(WR_VAR(0, 20, 12), c);
            add(WR_VAR(0, 20, 14), c);
            add(WR_VAR(0, 20, 17), c);
        }
    }

    // D02047  Direction of cloud drift
    void do_D02047(const Msg& msg, wreport::Subset& subset)
    {
        // D02047  Direction of cloud drift
        for (int i = 1; i <= 3; ++i)
        {
            if (const msg::Context* c = msg.find_context(Level::cloud(260, i), Trange::instant()))
            {
                if (const Var* var = c->find(WR_VAR(0,  8,  2)))
                    subset.store_variable(*var);
                else
                    subset.store_variable_undef(WR_VAR(0,  8,  2));

                if (const Var* var = c->find(WR_VAR(0, 20, 54)))
                    subset.store_variable(*var);
                else
                    subset.store_variable_undef(WR_VAR(0, 20, 54));
            } else {
                subset.store_variable_undef(WR_VAR(0,  8,  2));
                subset.store_variable_undef(WR_VAR(0, 20, 54));
            }
        }
    }

    // D02048  Direction and elevation of cloud
    void do_D02048(const Msg& msg, wreport::Subset& subset)
    {
        if (const msg::Context* c = msg.find_context(Level::cloud(262, 0), Trange::instant()))
        {
            if (const Var* var = c->find(WR_VAR(0,  5, 21)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0,  5, 21));

            if (const Var* var = c->find(WR_VAR(0,  7, 21)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0,  7, 21));

            if (const Var* var = c->find(WR_VAR(0, 20, 12)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0, 20, 12));
        } else {
            subset.store_variable_undef(WR_VAR(0,  5, 21));
            subset.store_variable_undef(WR_VAR(0,  7, 21));
            subset.store_variable_undef(WR_VAR(0, 20, 12));
        }
        subset.store_variable_undef(WR_VAR(0,  5, 21));
        subset.store_variable_undef(WR_VAR(0,  7, 21));
    }

    // D02037  State of ground, snow depth, ground minimum temperature
    void do_D02037(const Msg& msg, wreport::Subset& subset)
    {
        add(WR_VAR(0, 20,  62), DBA_MSG_STATE_GROUND);
        add(WR_VAR(0, 13,  13), DBA_MSG_TOT_SNOW);
        if (const Var* var = msg.find(WR_VAR(0, 12, 121), Level(1), Trange(3, 0, 43200)))
            subset.store_variable(WR_VAR(0, 12, 113), *var);
        else
            subset.store_variable_undef(WR_VAR(0, 12, 113));
    }

    void do_wind_gust(const msg::Context* c)
    {
        if (c)
        {
            // Compute time range from level and attrs
            const Var* var_dir = c->find(WR_VAR(0, 11, 43));
            const Var* var_speed = c->find(WR_VAR(0, 11, 41));
            int tr = MISSING_INT;
            if (c->trange.pind == 205)
                tr = c->trange.p2;
            else if (c->trange.pind == 254)
                tr = 600;
            tr = override_trange(var_dir, override_trange(var_speed, tr));
            if (tr == MISSING_INT)
                subset->store_variable_undef(WR_VAR(0,  4, 25));
            else
                subset->store_variable_d(WR_VAR(0,  4, 25), -tr / 60.0);

            add(WR_VAR(0, 11, 43), var_dir);
            add(WR_VAR(0, 11, 41), var_speed);
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 25));
            subset->store_variable_undef(WR_VAR(0, 11, 43));
            subset->store_variable_undef(WR_VAR(0, 11, 41));
        }
    }

    void add_time_period(Varcode code, const msg::Context& c, const Var* sample_var, const Trange& tr_std)
    {
        int p2;
        if (c.trange.pind == 254)
            p2 = tr_std.p2;
        else
            p2 = c.trange.p2;

        // Look for a p2 override in the attributes
        if (sample_var)
            if (const Var* a = sample_var->enqa(WR_VAR(0, 4, 194)))
                p2 = a->enqi();

        if (p2 == MISSING_INT)
        {
            subset->store_variable_undef(WR_VAR(0, 4, 24));
            return;
        }

        double res = -p2;
        switch (code)
        {
            case WR_VAR(0, 4, 24): res /= 3600.0; break;
            case WR_VAR(0, 4, 25): res /= 60.0; break;
        }
        subset->store_variable_d(code, res);
    }

    void do_prec_group(const msg::Context* c)
    {
        if (c)
        {
            if (c->trange.p2 != MISSING_INT)
                subset->store_variable_d(WR_VAR(0,  4, 24), -c->trange.p2 / 3600);
            else
                subset->store_variable_undef(WR_VAR(0,  4, 24));
            if (const Var* var = c->find(WR_VAR(0, 13, 11)))
                subset->store_variable(*var);
            else
                subset->store_variable_undef(WR_VAR(0, 13, 11));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0, 13, 11));
        }
    }

    void do_xtemp_group(Varcode code, const msg::Context* c)
    {
        if (c)
        {
            // Duration of statistical processing
            if (c->trange.p2 != MISSING_INT)
                subset->store_variable_d(WR_VAR(0,  4, 24), -c->trange.p2 / 3600);
            else
                subset->store_variable_undef(WR_VAR(0,  4, 24));

            // Offset from end of interval to synop reference time
            if (c->trange.p1 != 0 && c->trange.p1 != MISSING_INT)
                subset->store_variable_d(WR_VAR(0,  4, 24), c->trange.p1 / 3600);
            else if (c->trange.p1 == MISSING_INT || c->trange.p2 == MISSING_INT)
                subset->store_variable_undef(WR_VAR(0,  4, 24));
            else
                subset->store_variable_d(WR_VAR(0,  4, 24), 0);

            add(code, c, WR_VAR(0, 12, 101));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(code);
        }
    }

    // D02043  Basic synoptic "period" data
    void do_D02043(int hour)
    {
        //   Present and past weather
        add(WR_VAR(0, 20,  3), DBA_MSG_PRES_WTR);
        if (c_past_wtr)
        {
            // Look for a p2 override in the attributes
            const Var* v = c_past_wtr->find(WR_VAR(0, 20, 4));
            if (!v) v = c_past_wtr->find(WR_VAR(0, 20, 5));
            add_time_period(WR_VAR(0, 4, 24), *c_past_wtr, v, hour % 6 == 0 ? tr_std_past_wtr6 : tr_std_past_wtr3);
        } else
            subset->store_variable_undef(WR_VAR(0, 4, 24));
        add(WR_VAR(0, 20, 4), c_past_wtr, DBA_MSG_PAST_WTR1_6H);
        add(WR_VAR(0, 20, 5), c_past_wtr, DBA_MSG_PAST_WTR2_6H);

        //   Sunshine data (form 1 hour and 24 hour period)
        if (c_sunshine1)
        {
            subset->store_variable_d(WR_VAR(0,  4, 24), -c_sunshine1->trange.p2 / 3600);
            if (const Var* var = c_sunshine1->find(WR_VAR(0, 14, 31)))
                subset->store_variable(*var);
            else
                subset->store_variable_undef(WR_VAR(0, 14, 31));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0, 14, 31));
        }
        if (c_sunshine2)
        {
            subset->store_variable_d(WR_VAR(0,  4, 24), -c_sunshine2->trange.p2 / 3600);
            if (const Var* var = c_sunshine2->find(WR_VAR(0, 14, 31)))
                subset->store_variable(*var);
            else
                subset->store_variable_undef(WR_VAR(0, 14, 31));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0, 14, 31));
        }

        //   Precipitation measurement
        if (i_prec.c_prec1)
        {
            const Var* prec_var = i_prec.c_prec1->find(WR_VAR(0, 13, 11));
            add_sensor_height(*i_prec.c_prec1, prec_var);
        } else {
            subset->store_variable_undef(WR_VAR(0,  7, 32));
        }
        do_prec_group(i_prec.c_prec1);
        do_prec_group(i_prec.c_prec2);

        //   Extreme temperature data
        if (c_tmax || c_tmin)
        {
            const msg::Context* c_first = c_tmax ? c_tmax : c_tmin;
            add_sensor_height(*c_first, c_first->find(WR_VAR(0, 12, 101)));
        }
        else
            subset->store_variable_undef(WR_VAR(0,  7, 32));
        do_xtemp_group(WR_VAR(0, 12, 111), c_tmax);
        do_xtemp_group(WR_VAR(0, 12, 112), c_tmin);

        //   Wind data
        if (c_wind || c_gust1 || c_gust2)
        {
            // Look for the sensor height in the context of any of the found levels
            const msg::Context* c_first = c_wind ? c_wind : c_gust1 ? c_gust1 : c_gust2;
            const Var* sample_var = c_first->find(WR_VAR(0, 11, 1));
            if (!sample_var) sample_var = c_first->find(WR_VAR(0, 11, 2));
            if (!sample_var) sample_var = c_first->find(WR_VAR(0, 11, 41));
            if (!sample_var) sample_var = c_first->find(WR_VAR(0, 11, 43));
            add_sensor_height(*c_first, sample_var);

            add(WR_VAR(0, 2, 2), DBA_MSG_WIND_INST);
            subset->store_variable_i(WR_VAR(0, 8, 21), 2);

            if (c_wind)
            {
                // Compute time range from level and attrs
                const Var* var_dir = c_wind->find(WR_VAR(0, 11, 1));
                const Var* var_speed = c_wind->find(WR_VAR(0, 11, 2));
                int tr = MISSING_INT;
                if (c_wind->trange.pind == 200)
                    tr = c_wind->trange.p2;
                tr = override_trange(var_dir, override_trange(var_speed, tr));
                if (tr == MISSING_INT)
                    subset->store_variable_undef(WR_VAR(0,  4, 25));
                else
                    subset->store_variable_d(WR_VAR(0,  4, 25), -tr / 60.0);

                add(WR_VAR(0, 11,  1), var_dir);
                add(WR_VAR(0, 11,  2), var_speed);
            } else {
                subset->store_variable_undef(WR_VAR(0,  4, 25));
                subset->store_variable_undef(WR_VAR(0, 11,  1));
                subset->store_variable_undef(WR_VAR(0, 11,  2));
            }
            subset->store_variable_undef(WR_VAR(0,  8, 21));

            do_wind_gust(c_gust1);
            do_wind_gust(c_gust2);
        } else {
            subset->store_variable_undef(WR_VAR(0,  7, 32));
            subset->store_variable_undef(WR_VAR(0,  2,  2));
            subset->store_variable_i(WR_VAR(0,  8, 21), 2);
            subset->store_variable_undef(WR_VAR(0,  4, 25));
            subset->store_variable_undef(WR_VAR(0, 11,  1));
            subset->store_variable_undef(WR_VAR(0, 11,  2));
            subset->store_variable_undef(WR_VAR(0,  8, 21));
            for (int i = 1; i <= 2; ++i)
            {
                subset->store_variable_undef(WR_VAR(0,  4, 25));
                subset->store_variable_undef(WR_VAR(0, 11, 43));
                subset->store_variable_undef(WR_VAR(0, 11, 41));
            }
        }
        subset->store_variable_undef(WR_VAR(0,  7, 32));
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Synop::to_subset(msg, subset);

        // D01090  Fixed surface identification, time, horizontal and vertical coordinates
        do_D01004(); // station id
        do_D01011(); // date
        int hour = do_D01012(); // time
        do_D01021(); // coordinates
        add(WR_VAR(0,  7, 30), DBA_MSG_HEIGHT_STATION);
        add(WR_VAR(0,  7, 31), DBA_MSG_HEIGHT_BARO);
        // D02031  Pressure data
        do_D02031(msg, subset);
        // D02035  Basis synoptic "instantaneous" data
        do_D02035(msg, subset);
        // D02036  Clouds with bases below station level
        do_D02036(msg, subset);
        // D02047  Direction of cloud drift
        do_D02047(msg, subset);
        // B08002
        subset.store_variable_undef(WR_VAR(0, 8, 2));
        // D02048  Direction and elevation of cloud
        do_D02048(msg, subset);
        // D02037  State of ground, snow depth, ground minimum temperature
        do_D02037(msg, subset);
        // D02043  Basic synoptic "period" data
        do_D02043(hour);

        // D02044  Evaporation data
        if (c_evapo)
        {
            if (c_evapo->trange.p1 == 0)
                subset.store_variable_d(WR_VAR(0,  4, 24), -c_evapo->trange.p2/3600);
            else
                subset.store_variable_undef(WR_VAR(0,  4, 24));
            add(WR_VAR(0,  2,  4), WR_VAR(0,  2,  4), Level(1), Trange::instant());
        } else {
            subset.store_variable_undef(WR_VAR(0,  4, 24));
            add(WR_VAR(0,  2,  4), WR_VAR(0,  2,  4), Level(1), Trange::instant());
        }
        add(WR_VAR(0, 13, 33), c_evapo);

#warning TODO
        // D02045  Radiation data (1 hour period)
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  2)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  4)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 16)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 28)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 29)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 30)); // TODO
        // D02045  Radiation data (24 hour period)
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  2)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  4)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 16)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 28)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 29)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 30)); // TODO
        // D02046  Temperature change
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 12, 49)); // TODO
    }
};


struct SynopWMOFactory : public virtual TemplateFactory
{
    SynopWMOFactory() { name = SYNOP_WMO_NAME; description = SYNOP_WMO_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        // Scan msgs and pick the right one
        return auto_ptr<Template>(new SynopWMO(opts, msgs));
    }
};

struct SynopECMWFLandFactory : public virtual TemplateFactory
{
    SynopECMWFLandFactory() { name = SYNOP_ECMWF_LAND_NAME; description = SYNOP_ECMWF_LAND_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return auto_ptr<Template>(new SynopECMWFLand(opts, msgs));
    }
};

struct SynopECMWFLandHighFactory : public virtual TemplateFactory
{
    SynopECMWFLandHighFactory() { name = SYNOP_ECMWF_LAND_HIGH_NAME; description = SYNOP_ECMWF_LAND_HIGH_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return auto_ptr<Template>(new SynopECMWFLandHigh(opts, msgs));
    }
};

struct SynopECMWFAutoFactory : public virtual TemplateFactory
{
    SynopECMWFAutoFactory() { name = SYNOP_ECMWF_AUTO_NAME; description = SYNOP_ECMWF_AUTO_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return auto_ptr<Template>(new SynopECMWFAuto(opts, msgs));
    }
};

struct SynopECMWFFactory : public virtual TemplateFactory
{
    SynopECMWFFactory() { name = SYNOP_ECMWF_NAME; description = SYNOP_ECMWF_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        const Msg& msg = *msgs[0];
        const Var* var = msg.get_st_type_var();
        if (var != NULL && var->enqi() == 0)
            return auto_ptr<Template>(new SynopECMWFAuto(opts, msgs));

        ContextFinder finder(msg);
        finder.add_var(WR_VAR(0, 10, 8));
        if (finder.find_in_level(100))
            return auto_ptr<Template>(new SynopECMWFLandHigh(opts, msgs));

        return auto_ptr<Template>(new SynopECMWFLand(opts, msgs));
    }
};

struct SynopFactory : public SynopECMWFFactory, SynopWMOFactory
{
    SynopFactory() { name = SYNOP_NAME; description = SYNOP_DESC; }

    std::auto_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        const Msg& msg = *msgs[0];
        const Var* var = msg.get_st_name_var();
        if (var)
            return SynopWMOFactory::make(opts, msgs);
        else
            return SynopECMWFFactory::make(opts, msgs);
    }
};


} // anonymous namespace

void register_synop(TemplateRegistry& r)
{
static const TemplateFactory* synop = NULL;
static const TemplateFactory* synopwmo = NULL;
static const TemplateFactory* synopecmwf = NULL;
static const TemplateFactory* synopecmwfland = NULL;
static const TemplateFactory* synopecmwflandhigh = NULL;
static const TemplateFactory* synopecmwfauto = NULL;

    if (!synop) synop = new SynopFactory;
    if (!synopwmo) synopwmo = new SynopWMOFactory;
    if (!synopecmwf) synopecmwf = new SynopECMWFFactory;
    if (!synopecmwfland) synopecmwfland = new SynopECMWFLandFactory;
    if (!synopecmwflandhigh) synopecmwflandhigh = new SynopECMWFLandHighFactory;
    if (!synopecmwfauto) synopecmwfauto = new SynopECMWFAutoFactory;

    r.register_factory(synop);
    r.register_factory(synopwmo);
    r.register_factory(synopecmwf);
    r.register_factory(synopecmwfland);
    r.register_factory(synopecmwflandhigh);
    r.register_factory(synopecmwfauto);
}

}
}
}
/* vim:set ts=4 sw=4: */
