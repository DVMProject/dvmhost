# Example SQLite Queries for Grafana

## Overview

This document contains a few example SQLite queries usable with Grafana to create metrics visualizations. These are examples and no support is given for them, they were all generated
against Grafana 11.

## Examples

### Example 1: Completed Call Events

- Visualization: Gauge

Query:
```sql
WITH bounds AS (
  SELECT
    ($__from * 1000000) AS start_ns,
    ($__to * 1000000) AS stop_ns
)
SELECT COUNT(*) AS complete_call_event
FROM call_event c, bounds b
WHERE c.ts_ns >= b.start_ns
  AND c.ts_ns < b.stop_ns;
```

### Example 2: Active Talkgroup Count

- Visualization: Gauge

Query:
```sql
WITH bounds AS (
  SELECT
    ($__from * 1000000) AS start_ns,
    ($__to * 1000000) AS stop_ns
)
SELECT COUNT(DISTINCT c.dst_id) AS "Active TG Count"
FROM call_event c, bounds b
WHERE c.ts_ns >= b.start_ns
AND c.ts_ns < b.stop_ns;
```

### Example 3: Active RID Count

- Visualization; Gauge

Query:
```sql
WITH bounds AS (
  SELECT
    ($__from * 1000000) AS start_ns,
    ($__to * 1000000) AS stop_ns
)
SELECT COUNT(DISTINCT c.src_id) AS "Active RID Count"
FROM call_event c, bounds b
WHERE c.ts_ns >= b.start_ns
AND c.ts_ns < b.stop_ns;
```

### Example 4: Call Volume

- Visualization: Time Series

Query:
```sql
WITH bounds AS (
  SELECT
    ($__from / 1000) AS start_ns,
    ($__to / 1000) AS stop_ns
)
SELECT
  CAST(ts_s / 300 AS INTEGER) * 300 AS time,
  COUNT(*) AS "Call Volume"
FROM call_event, bounds b
WHERE ts_s BETWEEN b.start_ns AND b.stop_ns
GROUP BY time
ORDER BY time;
```
