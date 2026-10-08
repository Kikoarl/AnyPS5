import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions


class CheckConventionsTests(unittest.TestCase):
    def test_conventional_commits_regex(self):
        valid = [
            "feat: implement new opcode",
            "fix(kernel): resolve mutex race",
            "docs(user): clarify build instructions",
            "style(audio): remove comments",
            "refactor(libc): simplify heap allocation",
            "perf(agc): optimize draw call buffer",
            "test(shader): add regression test",
            "build: update cmake dependencies",
            "ci: configure workflow trigger",
            "chore: bump submodule version",
            "revert: rollback bad commit",
            "feat(shader)!: breaking ISA change",
        ]
        for subject in valid:
            with self.subTest(subject=subject):
                self.assertIsNotNone(
                    check_conventions.CONVENTIONAL.match(subject),
                    f"Expected '{subject}' to match Conventional Commits format",
                )

        invalid = [
            "Implemented new opcode",
            "fix:no-space-after-colon",
            "WIP: working on tests",
            "Update README.md",
            "just a commit message",
            "feat:",
            "fix(): empty scope",
        ]
        for subject in invalid:
            with self.subTest(subject=subject):
                self.assertIsNone(
                    check_conventions.CONVENTIONAL.match(subject),
                    f"Expected '{subject}' to fail Conventional Commits check",
                )

    def test_allowed_notes(self):
        for agent in check_conventions.AGENT_FILES:
            with self.subTest(agent=agent):
                self.assertFalse(check_conventions.allowed_notes(agent))
                self.assertFalse(check_conventions.allowed_notes(f"sub/{agent}"))

        valid_docs = [
            "README.md",
            "CONTRIBUTING.md",
            ".github/pull_request_template.md",
            "docs/dev/CONVENTIONS.md",
            "docs/dev/TechnicalDebt.md",
            "docs/user/USAGE.md",
        ]
        for path in valid_docs:
            with self.subTest(path=path):
                self.assertTrue(check_conventions.allowed_notes(path))

        rogue_notes = [
            "notes.md",
            "core/investigation.markdown",
            "scratch/debug.log",
            "core/libs/patch.diff",
        ]
        for path in rogue_notes:
            with self.subTest(path=path):
                self.assertFalse(check_conventions.allowed_notes(path))

        non_notes = [
            "core/src/main.cpp",
            ".github/workflows/build.yml",
            "CMakeLists.txt",
        ]
        for path in non_notes:
            with self.subTest(path=path):
                self.assertIsNone(check_conventions.allowed_notes(path))

    def test_function_body(self):
        lines = [
            "int Simple() {",
            "    return 42;",
            "}",
            "void WithBlockComments() {",
            "    /* block comment */",
            "    return 0;",
            "}",
            "void SemicolonBeforeBrace();",
        ]
        body = check_conventions.function_body(lines, 0)
        self.assertEqual(body, "return42;")

        body = check_conventions.function_body(lines, 3)
        self.assertEqual(body, "return0;")

        body = check_conventions.function_body(lines, 7)
        self.assertIsNone(body)

    def test_links_extraction_and_resolution(self):
        text = "See [conventions](CONVENTIONS.md) and [code](../../CONTRIBUTING.md#code)."
        resolved = check_conventions.links("docs/dev/README.md", text)
        targets = [r[1] for r in resolved]
        self.assertIn("docs/dev/CONVENTIONS.md", targets)
        self.assertIn("CONTRIBUTING.md", targets)

    def test_library_name_extraction(self):
        self.assertEqual(
            check_conventions.library("core/libs/prx/libSceAudioOut/src/AudioOut.cpp"),
            "libSceAudioOut",
        )
        self.assertEqual(
            check_conventions.library("core/libs/prx/libkernel/File/src/Open.cpp"),
            "libkernel",
        )

    def test_stub_body_detection(self):
        stubs = [
            "return0;",
            "(void)x;return0;",
            "static_cast<void>(param);returnnullptr;",
            "returntrue;",
            "returnfalse;",
            "return-1;",
            "return0x80920007;",
        ]
        for body in stubs:
            with self.subTest(body=body):
                self.assertIsNotNone(check_conventions.STUB_BODY.match(body))

        implemented = [
            "throwstd::runtime_error(\"unimplemented\");",
            "x=y+1;returnx;",
            "DoSomething();return0;",
        ]
        for body in implemented:
            with self.subTest(body=body):
                self.assertIsNone(check_conventions.STUB_BODY.match(body))


if __name__ == "__main__":
    unittest.main()
