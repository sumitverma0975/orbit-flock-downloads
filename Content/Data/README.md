# Road data and source provenance

`delhi.json` and `chandigarh.json` contain actual OpenStreetMap road polylines
within 1,200 metres of each extract's centre. They are deliberately small mobile
map extracts, not complete maps of either city. The centre coordinates, retrieval
time, OSM database snapshot time, Overpass source URL and filtering counts are
recorded in each file's `metadata`.

- Delhi centre: Connaught Place, 28.6315° N, 77.2167° E.
- Chandigarh centre: 30.7333° N, 76.7794° E.
- `sample_grid.json` is an invented test grid, not a surveyed city.

## What is mapped and what is generated

Road point coordinates follow OSM geometry, clipped at the circular extract
boundary. The game flattens roads to ground level. Bridges and tunnels are
excluded by default because flattening them would create false junctions. Roads
explicitly marked with restricted motorcar access (`no` or `private`, respecting
more specific motorcar tags) are excluded. Access is still incomplete in OSM;
this game is not a navigation tool.

`w` is road width in metres. `width_source` distinguishes explicitly tagged OSM
widths from widths estimated from lane counts or road class. Surface, smoothness,
oneway and access tags are retained in each road's `tags`; absence of a tag means
unknown. The current traffic simulation may not implement all those tags.

`features` contains explicitly mapped barrier nodes, pedestrian crossings,
railway level crossings, traffic-calming nodes, and potholes only if directly
tagged `hazard=pothole`, `hazard=potholes`, or `pothole=yes`. Each has an OSM node
ID, original relevant tags and `source=explicit_osm_tag`. **These are database
observations, not verified current road conditions.** A missing pothole record
never means that a road has no potholes. Feature records are available to runtime
rendering; not every feature kind or original tag is simulated.

Random gameplay potholes, buildings, traffic vehicles, train schedules and
mission checkpoints are generated game content. Their positions and behaviour
do not claim to reproduce actual road conditions, buildings or live traffic.

## Refresh and test

Run from the project directory:

```sh
python Tools/fetch_osm_roads.py delhi --radius 1200
python Tools/fetch_osm_roads.py chandigarh --radius 1200
python -m unittest discover -s Tools/tests -v
```

The fetcher tries public Overpass mirrors, rejects failed/empty responses, clips
geometry to the requested radius, and atomically replaces the output. A network
failure does not replace a real map with sample data. `--input response.json`
converts a saved Overpass JSON response without network access.

The Unreal reader's original `roads: [{h,w,p}]`, `crossings: [[x,y]]`, and
`center: {lat,lon}` fields are preserved. `p` coordinates are centimetres with
X east and Y south. Extra provenance fields do not change that reader schema.

## Attribution and data licence

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
[Open Database Licence (ODbL) 1.0](https://opendatacommons.org/licenses/odbl/1-0/).
These extracted databases retain that attribution and licence. Game code and
independently generated artwork are separate from the OSM database licence.
Display the OSM attribution in the game and retain it when redistributing maps.
