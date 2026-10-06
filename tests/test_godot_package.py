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
            "LSEnvironment": dict(PACKAGE.MACOS_RUNTIME_ENV),
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

    def create_project(self, suffix=""):
        project = self.root / ("project" + suffix)
        managed = {"project.godot", "native.gdextension", "native/libtanks_sample.dylib",
                   "main.gd", "main.tscn", "art.gd", "frontend.gd", "report_preview.gd", "audio_bank.gd",
                   "ui_checks.gd", "arcade_ui_checks.gd", "parity_checks.gd", "licenses/LICENSE"}
        managed.update(PACKAGE.PRODUCTION_EXCLUDED_SOURCES)
        for name in sorted(managed):
            path = project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n")
            if name.endswith(".gd"):
                Path(str(path) + ".uid").write_text("uid://fixture\n")
        (project / "project.godot").write_text('[application]\nrun/main_scene="res://main.tscn"\n')
        (project / "main.tscn").write_text('[ext_resource type="Script" path="res://main.gd" id="1"]\n')
        (project / "main.gd").write_text('const UI = preload("res://ui_checks.gd")\n'
                                         'var grass = load("res://resources/textures/battlefield_grass.png")\n')
        (project / "frontend.gd").write_text('const Report = preload("res://report_preview.gd")\n'
                                              'const Font = preload("res://resources/fonts/arcade.fnt")\n')
        (project / "audio_bank.gd").write_text('const Fire = preload("res://resources/sounds/player_fired.ogg")\n')
        (project / "ui_checks.gd").write_text('const UI = preload("res://arcade_ui_checks.gd")\n'
                                               'const Parity = preload("res://parity_checks.gd")\n')
        for name in [".godot/extension_list.cfg", ".godot/global_script_class_cache.cfg",
                     ".godot/uid_cache.bin", ".godot/editor/layout.cfg", ".godot/shader_cache/heavy.bin"]:
            path = project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture\n")
        (project / ".godot/extension_list.cfg").write_text("res://native.gdextension\n")
        (project / ".godot/global_script_class_cache.cfg").write_text('list=[{"path": "res://art.gd"}]\n')
        # An unused UID path is deliberately retained, without decoding or
        # rewriting the engine's binary format.
        (project / ".godot/uid_cache.bin").write_bytes(b"\x00res://art_checks.gd\x00")
        imports = [("resources/textures/battlefield_grass.png", "ctex"),
                   ("resources/textures/urban_masonry.png", "ctex"),
                   ("resources/fonts/arcade.fnt", "fontdata"), ("resources/fonts/arcade.png", "ctex")]
        imports.extend((name, "oggvorbisstr") for name in PACKAGE.gate.staging.source_files()
                       if name.startswith("resources/sounds/") and name.endswith(".ogg"))
        for source, extension in imports:
            self.add_import(project, managed, source, extension)
        return project, managed

    def add_import(self, project, managed, name, extension):
        managed.add(name)
        path = project / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("source " + name)
        output = ".godot/imported/" + path.name + "-fixture." + extension
        generated = project / output
        generated.parent.mkdir(parents=True, exist_ok=True)
        generated.write_text("imported " + name)
        generated.with_suffix(".md5").write_text("checksum " + name)
        Path(str(path) + ".import").write_text(
            '[remap]\nimporter="fixture"\npath=' + json.dumps("res://" + output) +
            '\nmetadata={\n"vram_texture": false\n}\n\n[deps]\nsource_file=' + json.dumps("res://" + name) +
            '\ndest_files=[\n' + json.dumps("res://" + output) + '\n]\n\n[params]\nfixture=true\n')
        return output

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

    def test_resealed_candidate_must_retain_complete_external_notices(self):
        app, identity = self.create_candidate()
        (app / "Contents/Resources/licenses/SCons-MIT.txt").unlink()
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

    def test_controller_driver_scope_cannot_expand_or_disappear(self):
        app = self.create_app()
        path = app / "Contents/Info.plist"
        original = plistlib.loads(path.read_bytes())
        for value in [None, {}, {"SDL_JOYSTICK_HIDAPI": "0"},
                      {"SDL_JOYSTICK_HIDAPI_SWITCH": "0"},
                      {"SDL_HIDAPI_IGNORE_DEVICES": "0x057e/0x0000"}]:
            with self.subTest(value=value):
                info = dict(original)
                if value is None:
                    info.pop("LSEnvironment")
                else:
                    info["LSEnvironment"] = value
                path.write_bytes(plistlib.dumps(info))
                self.seal(app)
                with mock.patch.object(PACKAGE, "run", side_effect=self.command_fixture):
                    with self.assertRaisesRegex(RuntimeError, "only Switch Pro"):
                        PACKAGE.verify(app)

    def test_packaged_smoke_uses_launchservices_controller_environment(self):
        app = self.create_app()
        result = mock.Mock(returncode=0, stdout="fixture output")
        with mock.patch.object(PACKAGE, "WORK", self.root), \
                mock.patch.object(PACKAGE.subprocess, "run", return_value=result) as launch, \
                mock.patch.object(PACKAGE.gate, "validate_output"), \
                mock.patch.object(PACKAGE.gate, "validate_report", return_value={}), \
                mock.patch.object(PACKAGE.gate, "validate_ui_report", return_value=[]), \
                mock.patch.dict(PACKAGE.os.environ, {"TANKS3D_TEST_ENV": "preserved"}):
            checks = PACKAGE.smoke(app)
        self.assertEqual(launch.call_count, 2)
        for call in launch.call_args_list:
            self.assertEqual(call.kwargs["env"]["SDL_HIDAPI_IGNORE_DEVICES"], "0x057e/0x2009")
            self.assertEqual(call.kwargs["env"]["TANKS3D_TEST_ENV"], "preserved")
        for check in checks.values():
            self.assertEqual(check["runtime_environment"], PACKAGE.MACOS_RUNTIME_ENV)

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
        project, managed = self.create_project()
        names = {entry["target"] for entry in PACKAGE.pack_entries(project, managed)}
        self.assertIn("res://.godot/imported/battlefield_grass.png-fixture.ctex", names)
        self.assertIn("res://main.gd", names)
        self.assertNotIn("res://native/libtanks_sample.dylib", names)
        self.assertNotIn("res://.godot/editor/layout.cfg", names)
        self.assertNotIn("res://.godot/shader_cache/heavy.bin", names)

    def test_production_pack_retains_frontend_and_imported_assets(self):
        project, managed = self.create_project()
        entries = PACKAGE.pack_entries(project, managed)
        names = {entry["target"].removeprefix("res://") for entry in entries}
        required = {"report_preview.gd", "ui_checks.gd", "arcade_ui_checks.gd", "parity_checks.gd",
                    "resources/fonts/arcade.fnt", "resources/fonts/arcade.png"}
        for name in required:
            self.assertIn(name, names)
            self.assertIn(name + (".uid" if name.endswith(".gd") else ".import"), names)
        for output in ["battlefield_grass.png-fixture.ctex", "arcade.fnt-fixture.fontdata",
                       "arcade.png-fixture.ctex", "player_fired.ogg-fixture.oggvorbisstr"]:
            self.assertIn(".godot/imported/" + output, names)
            self.assertIn(".godot/imported/" + str(Path(output).with_suffix(".md5")), names)
        for name in PACKAGE.PRODUCTION_EXCLUDED_SOURCES:
            self.assertNotIn(name, names)
            self.assertNotIn(name + ".uid", names)
            self.assertNotIn(name + ".import", names)
            self.assertTrue((project / name).is_file())
        self.assertEqual(sum(name.endswith("_checks.gd") for name in PACKAGE.PRODUCTION_EXCLUDED_SOURCES), 11)
        self.assertFalse(any("urban_masonry" in name or name.startswith("licenses/") for name in names))
        self.assertEqual((project / "licenses/LICENSE").read_text(), "fixture\n")
        uid = next(entry for entry in entries if entry["target"] == "res://.godot/uid_cache.bin")
        self.assertEqual(uid["sha256"], PACKAGE.gate.digest(project / ".godot/uid_cache.bin"))

    def test_raw_audio_and_grass_use_verified_virtual_resource_paths(self):
        project, managed = self.create_project()
        hashes = {name: PACKAGE.gate.digest(project / name) for name in managed}
        entries = PACKAGE.pack_entries(project, hashes)
        names = {entry["target"].removeprefix("res://") for entry in entries}
        audio = {name for name in managed if name.startswith("resources/sounds/") and name.endswith(".ogg")}
        self.assertEqual(len(audio), 22)
        raw = audio | {"resources/textures/battlefield_grass.png"}
        self.assertFalse(raw & names)
        for name in raw:
            self.assertIn(name + ".import", names)
            self.assertTrue((project / name).is_file())
            extension = "ctex" if name.endswith(".png") else "oggvorbisstr"
            output = ".godot/imported/" + Path(name).name + "-fixture." + extension
            self.assertIn(output, names)
            self.assertIn(str(Path(output).with_suffix(".md5")), names)
        # The actual source code is unchanged: virtual load/preload paths are
        # accepted only after their sidecar remaps and generated files pass.
        self.assertIn('load("res://resources/textures/battlefield_grass.png")', (project / "main.gd").read_text())
        self.assertIn('preload("res://resources/sounds/player_fired.ogg")', (project / "audio_bank.gd").read_text())
        self.assertIn("resources/fonts/arcade.fnt", names)
        self.assertIn("resources/fonts/arcade.png", names)

    def test_imported_orphans_and_their_checksums_are_not_packed(self):
        project, managed = self.create_project()
        for name in ["stale.png-old.ctex", "stale.png-old.md5", "battlefield_grass.png-old.ctex"]:
            (project / ".godot/imported" / name).write_text("unused old import")
        names = {entry["target"] for entry in PACKAGE.pack_entries(project, managed)}
        self.assertFalse(any("-old." in name for name in names))

    def test_import_dependencies_cannot_be_missing_or_inconsistent(self):
        for source, extension in [("resources/textures/battlefield_grass.png", "ctex"),
                                  ("resources/sounds/player_fired.ogg", "oggvorbisstr")]:
            for problem in ("sidecar", "output", "source", "remap", "traversal", "nonimported"):
                with self.subTest(source=source, problem=problem):
                    project, managed = self.create_project(str(len(list(self.root.glob("project*")))))
                    sidecar = project / (source + ".import")
                    text = sidecar.read_text()
                    output = project / (".godot/imported/" + Path(source).name + "-fixture." + extension)
                    if problem == "sidecar":
                        sidecar.unlink()
                    elif problem == "output":
                        output.unlink()
                    elif problem == "source":
                        sidecar.write_text(text.replace('source_file="res://' + source + '"', 'source_file="res://wrong.png"'))
                    elif problem == "remap":
                        sidecar.write_text(text.replace('path="res://.godot/imported/', 'path="res://.godot/imported/wrong-'))
                    elif problem == "traversal":
                        sidecar.write_text(text.replace('res://.godot/imported/', 'res://.godot/imported/../'))
                    else:
                        sidecar.write_text(text.replace('res://.godot/imported/', 'res://resources/'))
                    with self.assertRaises(RuntimeError):
                        PACKAGE.pack_entries(project, managed)

    def test_runtime_and_metadata_cannot_reference_excluded_sources(self):
        for name, text in [("main.gd", 'const Check = preload("res://art_checks.gd")\n'),
                           ("main.gd", "var check = load('res://art_review.gd')\n"),
                           ("main.tscn", '[ext_resource path="res://art_review.gd" id="1"]\n'),
                           ("project.godot", 'run/main_scene="res://missing.tscn"\n'),
                           (".godot/global_script_class_cache.cfg", 'list=[{"path": "res://art_checks.gd"}]\n')]:
            with self.subTest(name=name, text=text):
                project, managed = self.create_project(str(len(list(self.root.glob("project*")))))
                (project / name).write_text(text)
                with self.assertRaisesRegex(RuntimeError, "excluded or missing"):
                    PACKAGE.pack_entries(project, managed)

    def test_managed_source_hashes_are_checked_before_packing(self):
        for name in ["report_preview.gd", "resources/textures/battlefield_grass.png",
                     "resources/sounds/player_fired.ogg", "art_checks.gd", "licenses/LICENSE",
                     "native/libtanks_sample.dylib"]:
            with self.subTest(name=name):
                project, managed = self.create_project(str(len(list(self.root.glob("project*")))))
                hashes = {name: PACKAGE.gate.digest(project / name) for name in managed}
                (project / name).write_text("changed after staging validation\n")
                with self.assertRaisesRegex(RuntimeError, "changed since validation"):
                    PACKAGE.pack_entries(project, hashes)

    def test_missing_raw_source_cannot_hide_behind_a_valid_import(self):
        for name in ["resources/textures/battlefield_grass.png", "resources/sounds/player_fired.ogg"]:
            with self.subTest(name=name):
                project, managed = self.create_project(str(len(list(self.root.glob("project*")))))
                (project / name).unlink()
                with self.assertRaisesRegex(RuntimeError, "missing managed source"):
                    PACKAGE.pack_entries(project, managed)

    def test_unknown_files_in_excluded_locations_are_rejected(self):
        for name in ["licenses/extra.txt", "native/extra.dylib", "art_checks.gd.import.uid", "extra_checks.gd",
                     "resources/sounds/unknown.ogg"]:
            with self.subTest(name=name):
                project, managed = self.create_project(str(len(list(self.root.glob("project*")))))
                path = project / name
                path.write_text("unknown source")
                with self.assertRaisesRegex(RuntimeError, "Unmanaged"):
                    PACKAGE.pack_entries(project, managed)

    def test_pack_without_explicit_map_uses_authoritative_staging_sources(self):
        project, managed = self.create_project()
        with mock.patch.object(PACKAGE.gate.staging, "source_files", return_value={name: project / name for name in managed}):
            PACKAGE.pack_entries(project)
            (project / "unknown.gd").write_text("unknown source")
            with self.assertRaisesRegex(RuntimeError, "Unmanaged"):
                PACKAGE.pack_entries(project)

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
