# Kublet App Ideas

A collection of app ideas for the Kublet 240x240 smart display. Ideas are tagged with complexity and whether they need a server backend.

---

## Productivity & Info

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

### Countdown Timer
Countdown to a specific date (vacation, launch day, birthday). Big number display with label. Configure date via NVS.
`standalone` `easy`

---

## Weather & Environment

### Air Quality Index
Show AQI value as a big number with color-coded background (green/yellow/orange/red/purple). Use OpenAQ or AirNow API.
`standalone` `easy`

### Sunrise / Sunset Clock
Display today's sunrise and sunset times with a sun arc visualization showing current position in the day. Orange/blue gradients.
`standalone` `medium`

### Phase of Moon
Display the current moon phase as a detailed pixel art lunar disc with phase name, illumination percentage, and next full/new moon date.
`standalone` `medium`

### UV Index
Show current UV index as a large number with color-coded severity (green/yellow/orange/red/violet). Include a recommendation (hat, sunscreen, stay inside).
`standalone` `easy`

---

## Fun & Visual

### Pixel Art Gallery
Cycle through hand-drawn or procedurally generated pixel art scenes. 240x240 is perfect for pixel art. Button advances to next piece.
`standalone` `easy`

### Game of Life
Conway's Game of Life running on a grid. Button randomizes the board. Mesmerizing to watch on a desk.
`standalone` `easy`

### Starfield
Classic starfield warp speed animation. Stars fly outward from center with trailing streaks. Button changes speed/color.
`standalone` `easy`

### Nyan Cat
Animated Nyan Cat flying across the screen with a rainbow trail. Pure nostalgia.
`standalone` `easy`

### DVD Screensaver
The classic bouncing DVD logo that changes color when it hits a corner. Surprisingly satisfying to watch.
`standalone` `easy`

### Fireworks
Procedural fireworks bursting with particle effects. Multiple colors, random launch positions, trailing sparks.
`standalone` `medium`

### Cellular Automata
Visualize various cellular automata rules (Rule 30, Rule 110, etc.) generating mesmerizing patterns. Button cycles rules.
`standalone` `easy`

### Snake
Classic snake game that plays itself (AI-controlled), or let the button change direction. Show score.
`standalone` `medium`

---

## Data & Dashboards

### Server Resource Monitor
Show CPU, memory, and disk usage of a server as bar charts or gauges. Server-side script reports stats via HTTP.
`server` `medium`

### Pi-hole Stats
Display total queries, blocked percentage, and top blocked domain from a Pi-hole instance. Pi-hole has a built-in API.
`standalone` `medium`

### Home Assistant Entity
Display the state of a Home Assistant entity (temperature sensor, door status, light state). HA has a REST API.
`standalone` `medium`

---

## Time & Clocks

### World Clock
Show 2-3 timezone clocks at once (e.g., your city + a teammate's city). Useful for distributed teams.
`standalone` `easy`

### Binary Clock
Display current time in binary. Nerdy desk piece. Columns of lit/unlit circles for hours, minutes, seconds.
`standalone` `easy`

### Fuzzy Clock
Display time as natural language ("quarter past three", "almost midnight"). Minimalist and charming.
`standalone` `easy`

### Tetris Clock
Display time digits made of falling Tetris pieces. Animated transitions when digits change.
`standalone` `medium`

---

## Interactive

### Dice Roller
Roll dice on button press with a satisfying tumbling animation. Show result as dot pattern (like a real die face). Support d6, d20 etc via long-press cycling.
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

## Sports & Fitness

### Live Sports Scores
Show live scores for NBA, NFL, MLB, or EPL. Auto-update during game time. Use ESPN or similar free API.
`standalone` `hard`

### Strava Stats
Display recent activity stats — distance, pace, elevation. Show a weekly summary with a mini activity chart.
`server` `medium`

### Formula 1 Standings
Show current F1 driver/constructor standings. Update after each race weekend. Button toggles drivers vs constructors.
`standalone` `medium`

---

## Space & Science

### ISS Tracker
Show the current position of the International Space Station on a mini world map. Display next visible pass for your location.
`standalone` `medium`

### Planetarium
Show tonight's visible planets and constellations based on location and time. Animated star twinkle.
`standalone` `hard`

---

## Transit & Travel

### Flight Tracker
Show nearby flights overhead using ADS-B data or FlightAware API. Display airline, altitude, destination.
`standalone` `hard`

### Transit Departures
Show next departures from your local transit stop (bus, subway, train). Many cities have open GTFS APIs.
`standalone` `medium`

---

## Social & Communication

### Duolingo Streak
Display your current Duolingo streak count with the owl mascot in pixel art. Motivational desk reminder.
`standalone` `medium`

### Reddit Top Post
Show the top post from a configured subreddit with title, score, and comment count. Button cycles subreddits.
`standalone` `medium`

### Twitch Live
Show if a configured streamer is live with viewer count and stream title. Twitch API is free.
`standalone` `medium`
