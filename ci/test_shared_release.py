"""Consumer authority remains explicit around the pinned shared transaction."""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_release_workflow_preserves_identity_gates_and_same_run_artifacts() -> None:
    workflow = (ROOT / ".github/workflows/build.yml").read_text()
    job = workflow.split("\n  publish-release:\n", 1)[1].split(
        "\n  publish-to-test-pypi:\n", 1
    )[0]
    for required in (
        "needs: [plan, ci-passed]",
        "github.repository == 'LedFx/aubio-ledfx'",
        "github.event_name == 'push'",
        "startsWith(github.ref, 'refs/tags/v')",
        "needs.ci-passed.result == 'success'",
        "!cancelled()",
        "needs.plan.result == 'success'",
        "needs.plan.outputs.release == 'true'",
        "name: pypi",
        "queue: max",
        "cancel-in-progress: false",
        "pattern: wheels-*",
        "permission-contents: write",
        "permission-attestations: write",
    ):
        assert required in job
    assert "run-id:" not in job
    assert job.count("id-token: write") == 1
    assert "name: cibw-sdist" in job
    pins = re.findall(
        r"uses: LedFx/release-ci/actions/release@([0-9a-f]{40}) # (v[0-9]+\.[0-9]+\.[0-9]+)\s*$",
        job,
        re.MULTILINE,
    )
    assert len(pins) == 3 and len(set(pins)) == 1
    assert job.count("uses: LedFx/release-ci/actions/release@") == 3
    assert job.count("policy: release-tools/.github/release-policy.json") == 3
    assert (
        job.index("phase: prepare")
        < job.index("uses: actions/attest@")
        < job.index("phase: check-upload")
        < job.index("uses: pypa/gh-action-pypi-publish@")
        < job.index("phase: finalize")
    )
    assert "bundle-path" in job and "release-snapshot.json" in job
    assert "softprops" not in workflow and "--clobber" not in workflow


def test_explicit_policy_keeps_cpython_portable_artifacts() -> None:
    policy = json.loads((ROOT / ".github/release-policy.json").read_text())
    assert policy["repository"] == "LedFx/aubio-ledfx"
    assert policy["workflow"] == ".github/workflows/build.yml"
    assert policy["python"]["project"] == "aubio-ledfx"
    tags = policy["python"]["wheel_tags"]
    assert len(tags) == len(set(tags)) == 25
    assert all(
        "abi3" not in tag and "musllinux" not in tag and "armv7" not in tag
        for tag in tags
    )
    assert any("manylinux_2_28_x86_64" in tag for tag in tags)
    assert policy["python"]["sdist"] == "aubio_ledfx-{version}.tar.gz"
    assert policy["github_assets"] == {"distributions": True, "files": []}
    assert policy["oci"] == []


def test_manual_testpypi_lane_is_unchanged_and_separate() -> None:
    import hashlib

    workflow = (ROOT / ".github/workflows/build.yml").read_text()
    manual = (
        "\n  publish-to-test-pypi:\n"
        + workflow.split("\n  publish-to-test-pypi:\n", 1)[1]
    )
    assert (
        hashlib.sha256(manual.encode()).hexdigest()
        == "b9e0cdd7af0597829d9732369ea854437ba501b72ee7443468164727246b23b1"
    )
    assert "LedFx/release-ci" not in manual
    assert "repository-url: https://test.pypi.org/legacy/" in manual


if __name__ == "__main__":
    test_release_workflow_preserves_identity_gates_and_same_run_artifacts()
    test_explicit_policy_keeps_cpython_portable_artifacts()
    test_manual_testpypi_lane_is_unchanged_and_separate()
    print("3 shared publication contracts passed")
