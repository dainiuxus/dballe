import dballe
import io
import unittest
import os
import re
import sys
import tempfile


def test_pathname(fname):
    if fname.startswith("."):
        return fname

    envdir = os.environ.get("DBA_TESTDATA", ".")
    return os.path.normpath(os.path.join(envdir, fname))


class TestFileRead(unittest.TestCase):
    def setUp(self):
        self.pathname = test_pathname("bufr/gts-acars-uk1.bufr")

    def assertContents(self, f, pathname=None):
        if pathname is None:
            pathname = re.escape(self.pathname)
        self.assertRegex(f.name, pathname)
        self.assertEqual(f.encoding, "BUFR")
        contents = list(f)
        self.assertEqual(len(contents), 1)
        msg = contents[0]
        self.assertEqual(msg.encoding, "BUFR")
        self.assertRegex(msg.pathname, pathname)
        self.assertEqual(msg.offset, 0)
        self.assertEqual(msg.index, 0)
        data = bytes(msg)
        self.assertTrue(data.startswith(b"BUFR"))
        self.assertTrue(data.endswith(b"7777"))

    def test_named(self):
        with dballe.File(self.pathname) as f:
            self.assertContents(f)
        self.assertEqual(f.encoding, "BUFR")

    def test_named_encoding(self):
        with dballe.File(self.pathname, "bufr") as f:
            self.assertContents(f)

    def test_fileno(self):
        with open(self.pathname, "rb") as fd:
            with dballe.File(fd) as f:
                self.assertContents(f)

    def test_fileno_encoding(self):
        with open(self.pathname, "rb") as fd:
            with dballe.File(fd, "BUFR") as f:
                self.assertContents(f)

    def test_byteio(self):
        with open(self.pathname, "rb") as read_fd:
            with io.BytesIO(read_fd.read()) as fd:
                with dballe.File(fd) as f:
                    self.assertContents(f, pathname=r"^<_io\.BytesIO object at")

    def test_byteio_encoding(self):
        with open(self.pathname, "rb") as read_fd:
            with io.BytesIO(read_fd.read()) as fd:
                with dballe.File(fd, "BUFR") as f:
                    self.assertContents(f, pathname=r"^<_io\.BytesIO object at")

    def test_refcounting(self):
        file = dballe.File(self.pathname)
        self.assertEqual(sys.getrefcount(file), 2)  # file, getrefcount
        with file as f:
            self.assertEqual(sys.getrefcount(file), 4)  # file, __enter__ result, f, getrefcount
            self.assertEqual(sys.getrefcount(f), 4)  # file, __enter__ result, f, getrefcount
            for msg in f:
                self.assertEqual(sys.getrefcount(f), 5)  # file, __enter__ result, f, __iter__ result, getrefcount
                self.assertEqual(sys.getrefcount(msg), 2)  # msg, getrefcount
                data = bytes(msg)
                self.assertEqual(sys.getrefcount(msg), 2)  # msg, getrefcount
                self.assertEqual(sys.getrefcount(data), 2)  # data, getrefcount
            self.assertEqual(sys.getrefcount(msg), 2)  # msg, getrefcount
            self.assertEqual(sys.getrefcount(f), 4)  # file, __enter__ result, f, getrefcount
        self.assertEqual(sys.getrefcount(file), 3)  # file, f, _getrefcount
        self.assertEqual(sys.getrefcount(f), 3)  # file, f, _getrefcount


class TestFileWrite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with dballe.File(test_pathname("bufr/gts-acars-uk1.bufr"), "rb") as file:
            cls.messages = list(file)

    def assertContentsOk(self, f):
        reread = list(f)
        self.assertEqual(self.messages, reread)

    def test_named(self):
        with tempfile.NamedTemporaryFile() as tf:
            with dballe.File(tf.name, "wb") as f:
                for msg in self.messages:
                    f.write(msg)

            with dballe.File(tf.name, "rb") as f:
                self.assertContentsOk(f)

    def test_named_encoding(self):
        with tempfile.NamedTemporaryFile() as tf:
            with dballe.File(tf.name, "wb", "BUFR") as f:
                for msg in self.messages:
                    f.write(msg)

            with dballe.File(tf.name, "rb", "BUFR") as f:
                self.assertContentsOk(f)

    def test_fileno(self):
        with tempfile.NamedTemporaryFile() as tf:
            with dballe.File(tf) as f:
                for msg in self.messages:
                    f.write(msg)
            tf.seek(0)
            with dballe.File(tf, "rb") as f:
                self.assertContentsOk(f)

    def test_fileno_encoding(self):
        with tempfile.NamedTemporaryFile() as tf:
            with dballe.File(tf, "wb", "BUFR") as f:
                for msg in self.messages:
                    f.write(msg)
            tf.seek(0)
            with dballe.File(tf, "rb", "BUFR") as f:
                self.assertContentsOk(f)

    def test_byteio(self):
        with io.BytesIO() as fd:
            with dballe.File(fd, "wb") as f:
                for msg in self.messages:
                    f.write(msg)
            fd.seek(0)
            with dballe.File(fd, "rb") as f:
                self.assertContentsOk(f)

    def test_byteio_encoding(self):
        with io.BytesIO() as fd:
            with dballe.File(fd, "wb", "BUFR") as f:
                for msg in self.messages:
                    f.write(msg)
            fd.seek(0)
            with dballe.File(fd, "rb", "BUFR") as f:
                self.assertContentsOk(f)


if __name__ == "__main__":
    from testlib import main
    main("test-file")
