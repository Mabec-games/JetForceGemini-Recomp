import json, tempfile, unittest, zipfile
from pathlib import Path
from scripts import package_beta as beta

class BetaPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.runtime=self.root/'runtime';self.runtime.mkdir()
        self.launcher=self.root/'launcher.exe';self.launcher.write_bytes(b'MZsynthetic-launcher')
        for name in beta.RUNTIME_FILES:(self.runtime/name).write_bytes(b'MZsynthetic-runtime')
        self.receipt=self.runtime/'jfg-native-boot.exe.support'
        self.receipt.write_text('build_dirty=0\nbuild_source=fixture\nbuild_runtime_sha256='+beta.digest(self.runtime/beta.RUNTIME_FILES[0])+'\n')
        self.output=self.root/'out'
    def package(self):return beta.package(self.launcher,self.runtime,self.output,'1.0.1-beta.1')
    def test_allowlist_excludes_private_files(self):
        for name in ('game.z64','jfg.flash','debug.pdb','texture.png'):(self.runtime/name).write_bytes(b'PRIVATE-CANARY')
        archive=self.package()
        with zipfile.ZipFile(archive) as z:
            self.assertIsNone(z.testzip())
            self.assertFalse({'game.z64','jfg.flash','debug.pdb','texture.png'} & set(z.namelist()))
            manifest=json.loads(z.read('jfg-package.json'))
            self.assertEqual({e['name'] for e in manifest['files']},set(beta.RUNTIME_FILES))
    def test_identity_mismatch_rejected(self):
        (self.runtime/beta.RUNTIME_FILES[0]).write_bytes(b'MZchanged')
        with self.assertRaisesRegex(ValueError,'differs'):self.package()
    def test_dirty_runtime_rejected(self):
        self.receipt.write_text(self.receipt.read_text().replace('build_dirty=0','build_dirty=1'))
        with self.assertRaisesRegex(ValueError,'clean runtime'):self.package()
    def test_missing_library_rejected(self):
        (self.runtime/'SDL2.dll').unlink()
        with self.assertRaisesRegex(ValueError,'Incomplete'):self.package()
    def test_destination_preserved(self):
        self.output.mkdir();(self.output/'sentinel').write_text('keep')
        with self.assertRaisesRegex(ValueError,'Preserve'):self.package()
        self.assertEqual((self.output/'sentinel').read_text(),'keep')
    def test_each_missing_crt_library_is_rejected(self):
        for name in beta.VC_RUNTIME_FILES:
            with self.subTest(name=name):
                path=self.runtime/name;data=path.read_bytes();path.unlink()
                with self.assertRaisesRegex(ValueError,'Incomplete'):self.package()
                path.write_bytes(data)
    def test_explicit_crt_directory_is_used(self):
        crt=self.root/'redist';crt.mkdir()
        for name in beta.VC_RUNTIME_FILES:
            (self.runtime/name).unlink();(crt/name).write_bytes(b'MZredistributable')
        archive=beta.package(self.launcher,self.runtime,self.output,'1.0.1-beta.2',crt)
        with zipfile.ZipFile(archive) as z:
            for name in beta.VC_RUNTIME_FILES:self.assertEqual(z.read(name),b'MZredistributable')
