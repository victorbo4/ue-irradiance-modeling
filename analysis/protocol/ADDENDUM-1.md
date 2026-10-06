# Addendum 1 to protocol-v1 — model S-geo

`PROTOCOL.md` is unchanged (its hash is in the `protocol-v1` tag). This note records one model that
was added after the protocol-v1 results had been seen.

**S-geo** = `solcast_dni_wm2 · geometric_factor + solcast_dhi_wm2 · sky_view_factor`, clipped at 0.
It is the formula of R with Solcast's DNI and DHI in place of the clear-sky ones. It has no fitted
parameters. Because it needs no training, it is added as a column on the saved predictions
(`python -m evaluation.posthoc`); nothing was re-run and no other column changes.

It was chosen after seeing the E1 and E4 results, not before. Its contrasts (S-geo against S, R, A,
P-sim, H, D) are reported next to the protocol ones and labelled as added afterwards.

S-geo was explored in the same round as five variants of the corrector (an exponential cloud term,
precipitable water in the formula and in XGBoost, and two direct/diffuse splits). They were screened on
the 43 development days, not on the test days; the code and its output are in
`analysis/exploration/variants_screen.py` and `variants_screen_output.md`. Only precipitable water in the
formula gave a gain over P-sim (about 1 W/m2); the others did not. None of them is part of the evaluation.
