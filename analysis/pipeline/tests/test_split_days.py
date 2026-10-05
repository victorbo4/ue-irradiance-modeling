"""The day selector: quotas, forced TFG days, P1-regime coverage, determinism."""
import numpy as np
import pandas as pd
import pytest

import split_days as sd


def _days(n_per_class=12):
    rows = []
    for ci, cls in enumerate(sd.QUOTA):
        for i in range(n_per_class):
            rows.append({"date": f"2025-{(i % 9) + 4:02d}-{ci * 6 + i // 9 + 1:02d}", "sky": cls,
                         "p1_shaded": i % 2 == 0, "test_eligible": True, "month": f"2025-{(i % 9) + 4:02d}"})
    d = pd.DataFrame(rows).set_index("date")
    # the three forced TFG days, one per class they would belong to
    for day, cls, shaded in [("2025-04-11", "rainy", False), ("2025-04-20", "mixed", False), ("2025-10-07", "sunny", True)]:
        d.loc[day] = {"sky": cls, "p1_shaded": shaded, "test_eligible": True, "month": day[:7]}
    return d


@pytest.mark.parametrize("k,clear,expected", [
    (0.39, 0.0, "rainy"), (0.40, 0.10, "cloudy"), (0.80, 0.24, "cloudy"),
    (0.80, 0.25, "mixed"), (0.99, 0.84, "mixed"), (0.99, 0.85, "sunny"), (0.30, 1.0, "rainy"),
])
def test_classify_boundaries(k, clear, expected):
    assert sd.classify(k, clear) == expected


def test_selection_meets_quotas_and_forces_tfg_days():
    d = _days()
    picks = sd.select_test_days(d, seed=1, n_draws=300)
    assert len(picks) == sum(sd.QUOTA.values()) == len(set(picks))
    assert pd.Series(d.loc[picks, "sky"]).value_counts().to_dict() == sd.QUOTA
    assert set(sd.TFG_DAYS) <= set(picks)


def test_every_class_sees_both_p1_regimes():
    d = _days()
    picks = sd.select_test_days(d, seed=2, n_draws=300)
    for cls in sd.QUOTA:
        s = d.loc[[p for p in picks if d.loc[p, "sky"] == cls], "p1_shaded"]
        assert s.any() and not s.all(), cls


def test_selection_is_deterministic_and_seed_dependent():
    d = _days()
    assert sd.select_test_days(d, seed=7, n_draws=200) == sd.select_test_days(d, seed=7, n_draws=200)
    assert sd.select_test_days(d, seed=7, n_draws=5) != sd.select_test_days(d, seed=8, n_draws=5)


def test_ineligible_days_are_never_picked():
    d = _days()
    banned = d.index[(d.sky == "sunny") & ~d.index.isin(sd.TFG_DAYS)][:6]
    d.loc[banned, "test_eligible"] = False
    picks = sd.select_test_days(d, seed=3, n_draws=300)
    assert not set(picks) & set(banned)


def test_ineligible_tfg_day_is_an_error():
    d = _days(); d.loc["2025-10-07", "test_eligible"] = False
    with pytest.raises(ValueError):
        sd.select_test_days(d, n_draws=10)
