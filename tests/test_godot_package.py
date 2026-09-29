"""Local app resource/dependency checks; no GUI or template download required."""

import importlib.util
import json
from pathlib import Path
import plistlib
import shutil
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("godot_package", ROOT / "scripts/package_godot_app.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class GodotPackageTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build/tests").mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="godot-package-", dir=ROOT / "build/tests")
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def create_app(self):
        app = self.root / "Fixture.app"
        paths = [f"Contents/MacOS/{PACKAGE.EXECUTABLE}", f"Contents/Resources/{PACKAGE.EXECUTABLE}.pck",
                 "Contents/Frameworks/libtanks_sample.dylib",
                 "Contents/Resources/licenses/LICENSE", "Contents/Resources/licenses/Godot-MIT.txt",
                 "Contents/Resources/licenses/ASSET_LICENSES.md", "Contents/Resources/licenses/THIRD_PARTY_NOTICES.md",
                 "Contents/Resources/licenses/Godot-COPYRIGHT.txt", "Contents/Resources/licenses/godot-cpp-MIT.txt",
                 "Contents/Resources/licenses/LICENSES/Zlib-raylib.txt",
                 "Contents/Resources/licenses/LICENSES/Raylib-6.0-dependencies.txt"]
        for name in paths:
            path = app / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture " + name)
        (app / "Contents/Info.plist").write_bytes(plistlib.dumps({
            "CFBundleExecutable": PACKAGE.EXECUTABLE, "CFBundleIdentifier": PACKAGE.BUNDLE_ID,
            "LSMinimumSystemVersion": "26.0", "NSLocalNetworkUsageDescription": "Connect peers for cooperative play."}))
        self.seal(app)
        return app

    def seal(self, app, identity=None):
        (app / "Contents/Resources/package-manifest.json").write_text(json.dumps({
            "minimum_macos": "26.0", "files_sha256": PACKAGE.expected_files(app),
            "release_identity": identity, "release_candidate": identity is not None}))

    def create_candidate(self):
        app = self.create_app()
        identity = {"version": "0.2.0", "build": "7", "commit": "a" * 40, "tag": "v0.2.0-godot.alpha.1"}
        path = app / "Contents/Info.plist"
        info = plistlib.loads(path.read_bytes())
        info.update(CFBundleIdentifier=PACKAGE.RELEASE_BUNDLE_ID, CFBundleShortVersionString=identity["version"],
                    CFBundleVersion=identity["build"], Tanks3DSourceCommit=identity["commit"],
                    Tanks3DSourceTag=identity["tag"], Tanks3DLocalDevelopmentBuild=False)
        path.write_bytes(plistlib.dumps(info))
        for name, source in PACKAGE.gate.staging.source_files().items():
            if name.startswith("licenses/"):
                target = app / "Contents/Resources" / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        self.seal(app, identity)
        return app, identity

    def test_candidate_checks_version_build_commit_tag_and_development_flag(self):
        app, identity = self.create_candidate()
        path = app / "Contents/Info.plist"
        info = plistlib.loads(path.read_bytes())
        with mock.patch.object(PACKAGE, "run", side_effect=self.command_fixture):
            PACKAGE.verify(app, identity)
            for key, value in [("CFBundleShortVersionString", "0.1.0"), ("CFBundleVersion", "6"),
                               ("Tanks3DSourceCommit", "b" * 40), ("Tanks3DSourceTag", "v0.2.0-alpha.1"),
                               ("Tanks3DLocalDevelopmentBuild", True)]:
                with self.subTest(key=key):
                    path.write_bytes(plistlib.dumps(dict(info, **{key: value})))
                    self.seal(app, identity)
                    with self.assertRaisesRegex(RuntimeError, "metadata"):
                        PACKAGE.verify(app, identity)

    def test_resealed_candidate_cannot_relabel_original_notices(self):
        app, identity = self.create_candidate()
        (app / "Contents/Resources/licenses/LICENSES/Zlib-raylib.txt").write_text("incorrect replacement")
        self.seal(app, identity)
        with self.assertRaisesRegex(RuntimeError, "tagged originals"):
            PACKAGE.verify(app, identity)

    def test_development_app_cannot_pass_as_candidate(self):
        app = self.create_app()
        identity = {"version": "0.2.0", "build": "7", "commit": "a" * 40, "tag": "v0.2.0-godot.alpha.1"}
        with self.assertRaisesRegex(RuntimeError, "attested source identity"):
            PACKAGE.verify(app, identity)

    def command_fixture(self, command):
        if command[0] == "lipo":
            return "arm64\n"
        if command[0] == "codesign":
            return ""
        if command[1] == "-D":
            return str(command[2]) + ":\n"
        if command[1] == "-l":
            return "Load command 10\n cmd LC_BUILD_VERSION\n minos 26.0\n sdk 27.0\n tool 3\n version 27037.1\n"
        return str(command[2]) + ":\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)\n"

    def test_valid_fixture_has_exact_files(self):
        app = self.create_app()
        with mock.patch.object(PACKAGE, "run", side_effect=self.command_fixture):
            PACKAGE.verify(app)

    def test_modified_pack_is_rejected(self):
        app = self.create_app()
        (app / f"Contents/Resources/{PACKAGE.EXECUTABLE}.pck").write_text("tampered")
        with self.assertRaisesRegex(RuntimeError, "manifest"):
            PACKAGE.verify(app)

    def test_legacy_raylib_runtime_cannot_return_even_with_resealed_manifest(self):
        app = self.create_app()
        (app / "Contents/Frameworks/libraylib.600.dylib").write_text("legacy runtime")
        self.seal(app)
        with self.assertRaisesRegex(RuntimeError, "no raylib runtime"):
            PACKAGE.verify(app)

    def test_missing_notice_is_rejected_even_if_manifest_rewritten(self):
        app = self.create_app()
        (app / "Contents/Resources/licenses/LICENSES/Zlib-raylib.txt").unlink()
        self.seal(app)
        with self.assertRaisesRegex(RuntimeError, "license"):
            PACKAGE.verify(app)

    def test_extra_untracked_resource_is_rejected(self):
        app = self.create_app()
        (app / "Contents/Resources/unexpected.txt").write_text("extra")
        with self.assertRaisesRegex(RuntimeError, "manifest"):
            PACKAGE.verify(app)

    def test_homebrew_dependency_is_not_portable(self):
        app = self.create_app()
        with self.assertRaisesRegex(RuntimeError, "Unbundled"):
            PACKAGE.check_dependency("/opt/homebrew/opt/raylib/lib/libraylib.600.dylib",
                                     app / "Contents/Frameworks/libtanks_sample.dylib", app)

    def test_loader_relative_bundled_dependency_passes(self):
        app = self.create_app()
        (app / "Contents/Frameworks/fixture.dylib").write_text("path-resolution fixture")
        PACKAGE.check_dependency("@loader_path/fixture.dylib",
                                 app / "Contents/Frameworks/libtanks_sample.dylib", app)

    def test_missing_loader_relative_dependency_fails(self):
        app = self.create_app()
        with self.assertRaisesRegex(RuntimeError, "Missing"):
            PACKAGE.check_dependency("@loader_path/missing.dylib",
                                     app / "Contents/Frameworks/libtanks_sample.dylib", app)

    def test_dependency_traversal_is_rejected(self):
        app = self.create_app()
        with self.assertRaisesRegex(RuntimeError, "Unsafe"):
            PACKAGE.check_dependency("@loader_path/../../../outside.dylib",
                                     app / "Contents/Frameworks/libtanks_sample.dylib", app)

    def test_ambiguous_rpath_dependency_is_rejected(self):
        app = self.create_app()
        with self.assertRaisesRegex(RuntimeError, "ambiguous"):
            PACKAGE.check_dependency("@rpath/libraylib.600.dylib",
                                     app / "Contents/Frameworks/libtanks_sample.dylib", app)

    def test_symlink_resource_is_rejected(self):
        app = self.create_app()
        (app / "Contents/Resources/linked").symlink_to(self.root)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            PACKAGE.expected_files(app)

    def test_install_id_is_not_treated_as_external_dependency(self):
        output = "libx:\n\t@rpath/libx.dylib (compatibility version 0.0.0, current version 0.0.0)\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)\n"
        self.assertEqual(PACKAGE.linked_libraries(output, "@rpath/libx.dylib"), ["/usr/lib/libSystem.B.dylib"])

    def test_deployment_version_uses_declared_minimum(self):
        self.assertEqual(PACKAGE.minimum_macos("cmd LC_BUILD_VERSION\n minos 26.0\n sdk 26.1\n"), "26.0")

    def test_deployment_version_ignores_linker_and_source_versions(self):
        output = ("Load command 10\n cmd LC_BUILD_VERSION\n minos 26.0\n sdk 27.0\n"
                  " tool 3\n version 27037.1\nLoad command 11\n cmd LC_SOURCE_VERSION\n version 99000.1\n")
        self.assertEqual(PACKAGE.minimum_macos(output), "26.0")
        self.assertEqual(PACKAGE.minimum_macos("cmd LC_VERSION_MIN_MACOSX\n version 10.15\n sdk 26.1\n"), "10.15")

    def test_manifest_cannot_lie_about_actual_binary_minimum(self):
        app = self.create_app()
        info_file = app / "Contents/Info.plist"
        info = plistlib.loads(info_file.read_bytes())
        info["LSMinimumSystemVersion"] = "27037.1"
        info_file.write_bytes(plistlib.dumps(info))
        self.seal(app)
        manifest_file = app / "Contents/Resources/package-manifest.json"
        manifest = json.loads(manifest_file.read_text())
        manifest["minimum_macos"] = "27037.1"
        manifest_file.write_text(json.dumps(manifest))
        with mock.patch.object(PACKAGE, "run", side_effect=self.command_fixture):
            with self.assertRaisesRegex(RuntimeError, "actual Mach-O"):
                PACKAGE.verify(app)

    def test_pck_excludes_native_code_and_editor_cache(self):
        project = self.root / "project"
        for name in ["project.godot", "native.gdextension", "native/libtanks_sample.dylib",
                     ".godot/extension_list.cfg", ".godot/imported/grass.ctex", ".godot/editor/layout.cfg",
                     ".godot/shader_cache/heavy.bin", "main.gd", "resources/sounds/fire.ogg.import"]:
            path = project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
        names = {entry["target"] for entry in PACKAGE.pack_entries(project)}
        self.assertIn("res://.godot/imported/grass.ctex", names)
        self.assertIn("res://main.gd", names)
        self.assertNotIn("res://native/libtanks_sample.dylib", names)
        self.assertNotIn("res://.godot/editor/layout.cfg", names)
        self.assertNotIn("res://.godot/shader_cache/heavy.bin", names)

    def test_unknown_staged_file_cannot_enter_pack(self):
        project = self.root / "project"
        project.mkdir()
        (project / "untracked.txt").write_text("not a source resource")
        with self.assertRaisesRegex(RuntimeError, "Unmanaged"):
            PACKAGE.pack_entries(project, {"project.godot"})

    def test_final_receipt_hashes_actual_app_executable(self):
        app = self.create_app()
        with mock.patch.object(PACKAGE, "WORK", self.root):
            PACKAGE.write_receipt(app, {"native": "fixture"})
        receipt = json.loads((self.root / "app-receipt.json").read_text())
        name = f"Contents/MacOS/{PACKAGE.EXECUTABLE}"
        self.assertEqual(receipt["files_sha256"][name], PACKAGE.gate.digest(app / name))
        self.assertEqual(receipt["headless_checks"], {"native": "fixture"})


if __name__ == "__main__":
    unittest.main()
