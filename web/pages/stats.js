function duration(seconds) {
  seconds = Math.max(0, Math.round(seconds || 0));
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (h > 0) return h + 'h ' + m + 'm';
  if (m > 0) return m + ' min';
  return seconds > 0 ? '< 1 min' : '0 min';
}

function el(tag, cls, text) {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  if (text !== undefined) node.textContent = text;
  return node;
}

function stat(value, label) {
  const box = el('div', 'qz-stat');
  box.appendChild(el('b', '', value));
  box.appendChild(el('span', '', label));
  return box;
}

function bars(container, rows) {
  const max = Math.max(1, ...rows.map((r) => r[1]));
  container.textContent = '';
  for (const [label, value] of rows) {
    const row = el('div', 'qz-bar-row');
    row.appendChild(el('span', '', label));
    const track = el('div', 'qz-bar-track');
    const fill = el('div', 'qz-bar-fill');
    fill.style.width = Math.round((value / max) * 100) + '%';
    track.appendChild(fill);
    row.appendChild(track);
    row.appendChild(el('span', 'qz-num', duration(value)));
    container.appendChild(row);
  }
}

async function loadStats() {
  const source = document.getElementById('statsSource');
  let data;
  try {
    const response = await fetch('/api/stats');
    if (!response.ok) throw new Error(response.status + ' ' + response.statusText);
    data = await response.json();
  } catch (error) {
    source.textContent = 'Could not load reading stats: ' + error.message;
    return;
  }

  const g = data.global || {};
  const device = data.device || {};
  source.textContent = (device.name || 'This reader') + (data.exportedAt ? ' · as of ' + data.exportedAt.replace('T', ' ') : '');

  const totals = document.getElementById('totals');
  totals.append(
    stat(duration(g.readingSeconds), 'reading time'),
    stat(String(g.completedBooks || 0), 'books finished'),
    stat(String(g.sessions || 0), 'sessions'),
    stat((g.pagesTurned || 0).toLocaleString(), 'pages turned'),
    stat((g.currentStreakDays || 0) + ' days', 'current streak'),
    stat((g.longestStreakDays || 0) + ' days', 'longest streak'),
  );

  const tod = g.timeOfDaySeconds || {};
  const dow = g.dayOfWeekSeconds || [];
  if (g.readingSeconds > 0) {
    document.getElementById('whenCard').hidden = false;
    bars(document.getElementById('timeOfDay'), [
      ['Morning', tod.morning || 0],
      ['Afternoon', tod.afternoon || 0],
      ['Evening', tod.evening || 0],
      ['Night', tod.night || 0],
    ]);
    const days = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];
    bars(document.getElementById('dayOfWeek'), days.map((d, i) => [d, dow[i] || 0]));
  }

  const books = (data.books || []).filter((b) => (b.readingSeconds || 0) > 0 || b.completed);
  books.sort((a, b) => (b.readingSeconds || 0) - (a.readingSeconds || 0));
  if (books.length > 0) {
    document.getElementById('booksCard').hidden = false;
    const list = document.getElementById('books');
    for (const book of books) {
      const row = el('div', 'qz-book');
      const left = el('div');
      const title = el('div', 'qz-book-title', book.title || book.path);
      if (book.completed) title.appendChild(el('span', 'qz-badge', 'Finished'));
      left.appendChild(title);
      const meta = [book.author, (book.sessions || 0) + ' sessions'];
      if (!book.completed && book.timeLeftSeconds > 0) meta.push(duration(book.timeLeftSeconds) + ' left');
      left.appendChild(el('div', 'qz-book-meta', meta.filter(Boolean).join(' · ')));
      row.appendChild(left);
      row.appendChild(el('div', 'qz-book-time', duration(book.readingSeconds)));
      list.appendChild(row);
    }
  }
}

window.onload = loadStats;
