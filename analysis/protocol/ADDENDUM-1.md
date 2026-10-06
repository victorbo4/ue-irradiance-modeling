# Addendum 1 to protocol-v1 — model S-geo

`PROTOCOL.md` is unchanged (its hash is in the `protocol-v1` tag). This note records one model that
was added after the protocol-v1 results had been seen.

**S-geo** = `solcast_dni_wm2 · geometric_factor + solcast_dhi_wm2 · sky_view_factor`, clipped at 0.
It is the formula of R with Solcast's DNI and DHI in place of the clear-sky ones. It has no fitted
parameters. Because it needs no training, it is added as a column on the saved predictions
(`python -m evaluation.posthoc`); nothing was re-run and no other column changes.

It was chosen after seeing the E1 and E4 results, not before. Its contrasts (S-geo against S, R, A,
P-sim, H, D) are reported next to the protocol ones and labelled as added afterwards.

Besides S-geo, about five other variants were tried locally while exploring the results; none of
them improved on the models above and they are not part of the evaluation.
