"""mudraka — C++ sEMG engine for Mudra Link raw SNC decoding (Python bindings).

Thin wrapper over the nanobind extension `_core`. Example::

    import numpy as np
    from mudraka import Stream, Config

    cfg = Config()                      # 3ch, ~834 Hz, 4 s ring
    s = Stream(cfg)
    s.feed(notification_bytes, recv_time_s)   # one BLE notification per call

    out = np.empty((cfg.profile.channels, 4096), dtype=np.int32)
    written, next_cursor, lost = s.latest_into(out)   # zero-copy into `out`
"""
from ._core import Config, Stats, Stream, StreamProfile, __version__

__all__ = ["Config", "Stats", "Stream", "StreamProfile", "__version__"]
