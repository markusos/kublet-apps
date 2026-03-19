# Kublet App Ideas

A collection of app ideas for the Kublet 240x240 smart display. Ideas are tagged with complexity and whether they need a server backend.

---

## Productivity & Info

### Pomodoro Timer
Countdown timer with work/break cycles. Button starts/pauses/skips. Show a big countdown with a progress ring, change color for work (red) vs break (green). No server needed.
`standalone` `easy`

### Calendar / Next Meeting
Show your next upcoming meeting with title, time, and a countdown. Color-code by urgency (green > 30 min, yellow < 30 min, red < 5 min).
`server` `medium`

### Daily Quote
Fetch and display a random inspirational or funny quote. Button cycles to next quote. Lots of free quote APIs available.
`standalone` `easy`

### Hacker News Top Story
Show the current #1 story on HN with title, score, and comment count. Button cycles through top 5. Use the official HN API (no auth needed).
`standalone` `medium`

### GitHub Notifications
Show unread notification count with repo name and type (issue, PR, review). Requires a personal access token stored in NVS.
`standalone` `medium`

### Build Status
Monitor CI/CD pipeline status (GitHub Actions, GitLab CI). Show green/red/yellow circle with repo name and last build time. Similar pattern to the icinga app.
`server` `medium`

---

## Weather & Environment

### Weather Display
Current temperature, condition icon (sun/cloud/rain drawn with shapes), and high/low. Use OpenWeatherMap free tier. Button toggles between today and tomorrow.
`standalone` `medium`

### Air Quality Index
Show AQI value as a big number with color-coded background (green/yellow/orange/red/purple). Good for desk placement near a window.
`server` `easy`

### Sunrise / Sunset Clock
Display today's sunrise and sunset times with a sun arc visualization showing current position in the day. Looks great with orange/blue gradients.
`standalone` `medium`

---

## Fun & Visual

### Matrix Rain
Classic falling green characters animation. Pure eye candy, no data needed. Button could change color or speed.
`standalone` `easy`

### Pixel Art Gallery
Cycle through hand-drawn or procedurally generated pixel art. 240x240 is perfect for pixel art. Button advances to next piece.
`standalone` `easy`

### Game of Life
Conway's Game of Life running on a grid. Button randomizes the board. Mesmerizing to watch on a desk.
`standalone` `easy`

### Lava Lamp
Procedural lava lamp simulation using metaballs or simplex noise. Smooth, colorful, ambient display.
`standalone` `medium`

### Maze Generator
Generate and animate maze creation (recursive backtracker looks great). Button generates a new maze. Could also animate solving it.
`standalone` `easy`

### Aquarium
Animated fish swimming across the screen with bubbles. Simple sprite-based animation. Very desk-friendly.
`standalone` `medium`

---

## Data & Dashboards

### Crypto Price Tracker
Similar to the stock app but for BTC/ETH. Show price, 24h change, and a mini sparkline. Many free crypto APIs available.
`server` `medium`

### Server Resource Monitor
Show CPU, memory, and disk usage of a server as bar charts or gauges. Server-side script reports stats via HTTP.
`server` `medium`

### Pi-hole Stats
Display total queries, blocked percentage, and top blocked domain from a Pi-hole instance. Pi-hole has a built-in API.
`standalone` `medium`

### Home Assistant Entity
Display the state of a Home Assistant entity (temperature sensor, door status, light state). HA has a REST API.
`standalone` `medium`

### Network Speed Test
Run periodic speed tests from the server and display download/upload speeds. Good "is my internet working" glanceable display.
`server` `medium`

---

## Time & Clocks

### Analog Clock
Classic analog clock face with hour/minute/second hands. Clean, always-useful desk display. Draw with arcs and lines.
`standalone` `easy`

### World Clock
Show 2-3 timezone clocks at once (e.g., your city + a teammate's city). Useful for distributed teams.
`standalone` `easy`

### Binary Clock
Display current time in binary. Nerdy desk piece. Columns of lit/unlit circles for hours, minutes, seconds.
`standalone` `easy`

### Countdown
Countdown to a specific date (vacation, launch day, birthday). Big number display with label. Configure date via NVS.
`standalone` `easy`

---

## Interactive

### Dice Roller
Roll dice on button press with a satisfying animation. Show result as dot pattern (like a real die face). Support d6, d20 etc via long-press cycling.
`standalone` `easy`

### Decision Maker
"Should I..." - press the button, get a yes/no/maybe with a spinning wheel animation. Simple but surprisingly useful.
`standalone` `easy`

### Reaction Time Game
Screen flashes a color, press the button as fast as possible. Display reaction time in milliseconds. Track best score in NVS.
`standalone` `easy`

### Simon Says
Classic memory game with colored quadrants. Button confirms sequence. Gets harder each round. Store high score in NVS.
`standalone` `medium`

---

## Music & Audio (visual only)

### Spotify Now Playing
Show currently playing track name, artist, and a progress bar. Requires Spotify API auth via server.
`server` `medium`

### Album Art Display
Fetch and display album art for the currently playing track. 240x240 is a perfect square for album covers.
`server` `hard`
