# coding: utf-8
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import shlex
import os
import sys
import unittest

def main(testname):
    args = os.environ.get("ARGS", None)
    if args is None:
        return unittest.main()

    args = shlex.split(args);
    if args[0] != testname:
        return 0

    argv = [sys.argv[0]] + args[1:]
    unittest.main(argv=argv)

def fill_volnd(db):
    import random
    import dballe
    import datetime

    # We want a predictable dataset
    random.seed(1)
    rattr = random.Random()
    rattr.seed(1)

    # Wipe the test database
    db.reset()

    attrs = dballe.Record()
    rec = dballe.Record(mobile=0)

    def contexts():
        # 2 networks
        for net in ('synop', 'temp'):
            # 6 stations
            for lat in (10., 20., 30.):
                for lon in (15., 25.):
                    yield net, lat, lon

    def dtrange(start, stop, delta):
        while (start < stop):
            yield start
            start += delta

    def everyxhours(x):
        return dtrange(
                datetime.datetime(2007, 1, 1, 0, 0, 0),
                datetime.datetime(2007, 1, 7, 0, 0, 0),
                datetime.timedelta(0, x*3600, 0))

    def maybe_insert(rec, aname):
        if random.random() <= 0.9:
            #print repr(rec)
            db.insert(rec, False, True)
            attrs.clear()
            attrs[aname] = rattr.random() * 100.
            for code in rec:
                db.attr_insert(code, attrs)

    # Enter some sample data
    for net, lat, lon in contexts():
        rec["rep_memo"] = net
        if net == 'synop':
            aname = 'B33007'
        else:
            aname = 'B33040'
        rec["lat"] = lat
        rec["lon"] = lon

        # 6 hours precipitations
        rec["level"] = (1,)
        rec["trange"] = (4, -21600, 0)
        for dt in everyxhours(6):
            rec["date"] = dt
            rec["B13011"] = random.random()*10.
            maybe_insert(rec, aname)

        # 12 hours precipitations at different times
        rec["level"] = (1,)
        rec["trange"] = (4, -43200, 0)
        for dt in everyxhours(12):
            rec["date"] = dt
            rec["B13011"] = random.random()*10.
            maybe_insert(rec, aname)

        # Randomly measured
        # precipitations on a different
        # (meaningless) level
        # At slightly off times
        rec["level"] = (3, 2)
        rec["trange"] = (4, -21600, 0)
        for dt in everyxhours(6):
            rec["date"] = (dt + datetime.timedelta(0, random.randint(-600, 600)))
            rec["B13011"] = random.random()*10.
            maybe_insert(rec, aname)
        del rec["B13011"]

        # Pressures every 12 hours
        rec["level"] = (1,)
        rec["trange"] = (0,)
        for dt in everyxhours(12):
            rec["date"] = dt
            rec["B10004"] = float(random.randint(70000, 105000))
            maybe_insert(rec, aname)
        del rec["B10004"]

    # Insert some pseudoana data for the station 1, to test
    # pseudoana export and mixed data types
    rec.clear()
    rec.update(ana_id=1, B01001=12, B01002=123, B01019="Test of long station name", rep_memo="synop")
    rec.set_station_context()
    db.insert(rec, False, True)