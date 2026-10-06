"""The add-on that writes the S-geo column into predictions saved before S-geo existed."""
import pandas as pd
import pytest

from evaluation import models as M
from evaluation import posthoc as PH
from evaluation import run as R
from evaluation import smoke

GRID = dict(grid_hd=smoke.SMOKE_GRID_HD, grid_c=smoke.SMOKE_GRID_C, n_jobs=2, log=lambda *a: None)


@pytest.fixture()
def old_results(tmp_path):
    """Results as they were before S-geo: a smoke run with the S-geo column dropped from every file."""
    df, split = smoke.make_dataset()
    R.run_all(df, split, tmp_path, variants=["main", "alt10", "clip3"], only=["E1", "E3", "E4:2025-05"], **GRID)
    for p in tmp_path.glob("*/*/predictions.csv"):
        pd.read_csv(p).drop(columns=["S-geo"], errors="ignore").to_csv(p, index=False)
    return tmp_path, df


def test_the_column_is_added_with_the_formula_and_the_old_text_is_untouched(old_results):
    results, df = old_results
    before = {p: p.read_text().splitlines() for p in results.glob("*/*/predictions.csv")}
    done = PH.add_s_geo(results, df)
    assert done == {"added": 6, "unchanged": 0}                       # main and alt10, three runs each
    for p, old_lines in before.items():
        new_lines = p.read_text().splitlines()
        if "/clip3/" in str(p):
            assert new_lines == old_lines                               # the clip variants refit only H and D: not touched at all
            continue
        assert len(new_lines) == len(old_lines)
        assert new_lines[0] == old_lines[0] + ",S-geo"
        for a, b in zip(old_lines[1:], new_lines[1:]):
            assert b.startswith(a + ",") and b.count(",") == a.count(",") + 1      # every old line is intact, one field added
        new = pd.read_csv(p, float_precision="round_trip")      # the default parser can be one digit off
        old = new.drop(columns="S-geo")
        j = old[["utc", "sensor"]].merge(df, on=["utc", "sensor"], how="left")
        assert (new["S-geo"].to_numpy() == M.solcast_geometry(j).clip(lower=0).to_numpy()).all()
        assert new["S-geo"].notna().all()                               # also on the tilted sensor


def test_running_it_twice_changes_nothing(old_results):
    results, df = old_results
    PH.add_s_geo(results, df)
    first = {p: p.read_bytes() for p in results.glob("*/*/predictions.csv")}
    assert PH.add_s_geo(results, df) == {"added": 0, "unchanged": 6}
    assert {p: p.read_bytes() for p in results.glob("*/*/predictions.csv")} == first


def test_a_different_column_already_there_is_an_error_not_an_overwrite(old_results):
    results, df = old_results
    PH.add_s_geo(results, df)
    p = results / "main" / "E1" / "predictions.csv"
    x = pd.read_csv(p); x["S-geo"] += 1.0; x.to_csv(p, index=False)
    with pytest.raises(ValueError, match="differs from the formula"):
        PH.add_s_geo(results, df)


def test_rows_missing_from_the_dataset_stop_it(old_results):
    results, df = old_results
    with pytest.raises(ValueError, match="without the Solcast or geometry"):
        PH.add_s_geo(results, df.iloc[:-50])


def test_it_ignores_runs_that_did_not_finish_and_records_itself_in_the_manifest(old_results):
    results, df = old_results
    (results / "main" / "E1" / "done.json").unlink()
    assert PH.add_s_geo(results, df)["added"] == 5
    R.write_manifest(results, ["main"], smoke=True, tag_verified=False, argv=[])
    PH.record_in_manifest(results, {"added": 5, "unchanged": 0})
    import json
    m = json.loads((results / "manifest.json").read_text())
    assert m["posthoc"]["S-geo"]["runs_written"] == 5 and m["tag_verified"] is False
