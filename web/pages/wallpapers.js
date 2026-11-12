const MODE_HINTS = {
  2: 'Shows your pinned wallpaper, or a random one from the folder.',
  5: 'Shows the book cover, and a wallpaper when the book has no cover.',
  12: 'Shows the book cover with your PNG overlay on top.',
  6: 'Shows the page you were reading with your PNG overlay on top.',
};

let info = { folder: '/.sleep', folderExists: false, pinned: '', mode: 0 };
let screen = { width: 480, height: 800 };
let fitMode = 'fill';
let previewToken = 0;
const previewUrls = [];
let existingNames = new Set();

function el(tag, cls, text) {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  if (text !== undefined) node.textContent = text;
  return node;
}

function joinPath(folder, name) {
  return (folder.endsWith('/') ? folder : folder + '/') + name;
}

function baseName(path) {
  return path ? path.substring(path.lastIndexOf('/') + 1) : '';
}

function isPng(name) {
  return /\.png$/i.test(name);
}

function isBmp(name) {
  return /\.bmp$/i.test(name);
}

async function postForm(url, fields) {
  const body = new URLSearchParams(fields);
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  });
  const text = await response.text();
  if (!response.ok) throw new Error(text || response.statusText);
  return text;
}

// ---- Sleep screen mode ----

function renderInfo() {
  for (const button of document.querySelectorAll('#modeSeg button')) {
    button.classList.toggle('active', Number(button.dataset.mode) === info.mode);
    button.setAttribute('aria-checked', Number(button.dataset.mode) === info.mode ? 'true' : 'false');
  }
  document.getElementById('modeHint').textContent =
    MODE_HINTS[info.mode] || 'Your reader uses a different sleep screen right now. Pick one above to use wallpapers.';
  document.getElementById('pinLine').textContent = info.pinned
    ? 'Pinned: ' + baseName(info.pinned)
    : 'No wallpaper pinned. The reader picks one at random.';
  document.getElementById('folderLine').textContent = info.folderExists
    ? 'In ' + info.folder + ' on the SD card.'
    : 'No wallpapers yet. Uploads go to ' + info.folder + '.';
  for (const tile of document.querySelectorAll('.qz-wall')) {
    const pinned = tile.dataset.path === info.pinned;
    tile.classList.toggle('pinned', pinned);
    const button = tile.querySelector('.qz-pin');
    button.textContent = pinned ? 'Pinned' : 'Use';
    button.classList.toggle('primary', pinned);
  }
}

async function loadInfo() {
  const response = await fetch('/api/wallpapers?_=' + Date.now());
  if (!response.ok) throw new Error(await response.text());
  info = await response.json();
  renderInfo();
}

async function setMode(mode) {
  try {
    info = JSON.parse(await postForm('/api/wallpapers/mode', { mode: String(mode) }));
    renderInfo();
  } catch (error) {
    alert('Could not change the sleep screen: ' + error.message);
  }
}

async function pin(path) {
  try {
    const wasMode = info.mode;
    info = JSON.parse(await postForm('/api/wallpapers/pin', { path }));
    renderInfo();
    if (path && info.mode !== wasMode) {
      document.getElementById('modeHint').textContent =
        'Switched to Page Overlay so this PNG shows. ' + (MODE_HINTS[info.mode] || '');
    }
  } catch (error) {
    alert('Could not pin this wallpaper: ' + error.message);
  }
}

// ---- Wallpaper list ----

async function loadPreviews(tiles, token) {
  // One image at a time: the reader serves files over a single connection.
  for (const tile of tiles) {
    if (token !== previewToken) return;
    const holder = tile.querySelector('.qz-wall-img');
    try {
      const response = await fetch('/download?path=' + encodeURIComponent(tile.dataset.path));
      if (!response.ok) throw new Error(response.statusText);
      const raw = await response.blob();
      const type = isPng(tile.dataset.path) ? 'image/png' : 'image/bmp';
      const url = URL.createObjectURL(new Blob([raw], { type }));
      previewUrls.push(url);
      if (token !== previewToken) return;
      const img = el('img');
      img.alt = baseName(tile.dataset.path);
      img.src = url;
      holder.textContent = '';
      holder.appendChild(img);
    } catch (error) {
      holder.textContent = 'No preview';
    }
  }
}

async function loadList() {
  const grid = document.getElementById('grid');
  previewToken += 1;
  while (previewUrls.length) URL.revokeObjectURL(previewUrls.pop());
  grid.textContent = '';
  existingNames = new Set();
  if (!info.folderExists) return;

  let entries = [];
  try {
    const response = await fetch('/api/files?path=' + encodeURIComponent(info.folder) + '&_=' + Date.now());
    if (!response.ok) throw new Error(response.statusText);
    entries = await response.json();
  } catch (error) {
    document.getElementById('folderLine').textContent = 'Could not read ' + info.folder + ': ' + error.message;
    return;
  }

  existingNames = new Set(entries.map((e) => e.name.toLowerCase()));
  const images = entries.filter((e) => !e.isDirectory && (isBmp(e.name) || isPng(e.name)));
  images.sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }));
  if (images.length === 0) {
    document.getElementById('folderLine').textContent = 'No wallpapers in ' + info.folder + ' yet.';
    return;
  }

  const tiles = [];
  for (const image of images) {
    const path = joinPath(info.folder, image.name);
    const tile = el('div', 'qz-wall');
    tile.dataset.path = path;
    tile.appendChild(el('div', 'qz-wall-img', 'Loading…'));
    tile.appendChild(el('div', 'qz-wall-name', image.name));
    tile.appendChild(el('div', 'qz-wall-tag', isPng(image.name) ? 'PNG · overlay' : 'BMP'));
    const actions = el('div', 'qz-wall-actions');
    const pinButton = el('button', 'qz-btn qz-pin', 'Use');
    pinButton.type = 'button';
    pinButton.onclick = () => pin(tile.classList.contains('pinned') ? '' : path);
    const deleteButton = el('button', 'qz-btn', 'Delete');
    deleteButton.type = 'button';
    deleteButton.onclick = () => removeWallpaper(path);
    actions.append(pinButton, deleteButton);
    tile.appendChild(actions);
    grid.appendChild(tile);
    tiles.push(tile);
  }
  renderInfo();
  loadPreviews(tiles, previewToken);
}

async function removeWallpaper(path) {
  if (!confirm('Delete ' + baseName(path) + ' from the reader?')) return;
  try {
    if (path === info.pinned) await postForm('/api/wallpapers/pin', { path: '' });
    await postForm('/delete', { path });
  } catch (error) {
    alert('Could not delete: ' + error.message);
  }
  await refresh();
}

async function refresh() {
  try {
    await loadInfo();
  } catch (error) {
    document.getElementById('folderLine').textContent = 'Could not reach the reader: ' + error.message;
    return;
  }
  await loadList();
}

// ---- Converting photos for the screen ----

function loadImage(file) {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(url);
      resolve(img);
    };
    img.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error('This browser cannot open that image'));
    };
    img.src = url;
  });
}

function drawToScreen(img, fit, background) {
  const canvas = document.createElement('canvas');
  canvas.width = screen.width;
  canvas.height = screen.height;
  const ctx = canvas.getContext('2d');
  if (background) {
    ctx.fillStyle = background;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }
  const scale =
    fit === 'fit'
      ? Math.min(canvas.width / img.naturalWidth, canvas.height / img.naturalHeight)
      : Math.max(canvas.width / img.naturalWidth, canvas.height / img.naturalHeight);
  const w = img.naturalWidth * scale;
  const h = img.naturalHeight * scale;
  ctx.imageSmoothingQuality = 'high';
  ctx.drawImage(img, (canvas.width - w) / 2, (canvas.height - h) / 2, w, h);
  return canvas;
}

// An 8-bit grayscale BMP: the reader dithers it to the panel's grays.
function canvasToGrayBmp(canvas) {
  const { width, height } = canvas;
  const pixels = canvas.getContext('2d').getImageData(0, 0, width, height).data;
  const rowSize = (width + 3) & ~3;
  const paletteSize = 256 * 4;
  const dataOffset = 14 + 40 + paletteSize;
  const fileSize = dataOffset + rowSize * height;
  const buffer = new ArrayBuffer(fileSize);
  const view = new DataView(buffer);
  const bytes = new Uint8Array(buffer);

  view.setUint8(0, 0x42);
  view.setUint8(1, 0x4d);
  view.setUint32(2, fileSize, true);
  view.setUint32(10, dataOffset, true);
  view.setUint32(14, 40, true);
  view.setInt32(18, width, true);
  view.setInt32(22, height, true);
  view.setUint16(26, 1, true);
  view.setUint16(28, 8, true);
  view.setUint32(30, 0, true);
  view.setUint32(34, rowSize * height, true);
  view.setInt32(38, 2835, true);
  view.setInt32(42, 2835, true);
  view.setUint32(46, 256, true);
  view.setUint32(50, 256, true);
  for (let i = 0; i < 256; i++) {
    const p = 54 + i * 4;
    bytes[p] = i;
    bytes[p + 1] = i;
    bytes[p + 2] = i;
  }
  for (let y = 0; y < height; y++) {
    const row = dataOffset + (height - 1 - y) * rowSize;
    for (let x = 0; x < width; x++) {
      const i = (y * width + x) * 4;
      bytes[row + x] = Math.round(0.299 * pixels[i] + 0.587 * pixels[i + 1] + 0.114 * pixels[i + 2]);
    }
  }
  return new Blob([buffer], { type: 'image/bmp' });
}

function canvasToPng(canvas) {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => (blob ? resolve(blob) : reject(new Error('PNG encoding failed'))), 'image/png');
  });
}

function safeName(name, extension) {
  const stem = (name.replace(/\.[^.]*$/, '').replace(/[^A-Za-z0-9 _\-()]+/g, '_').trim() || 'wallpaper').substring(0, 60);
  let candidate = stem + extension;
  for (let n = 2; existingNames.has(candidate.toLowerCase()); n++) candidate = stem + ' (' + n + ')' + extension;
  return candidate;
}

async function prepare(file) {
  if (isBmp(file.name)) return { blob: file, name: safeName(file.name, '.bmp') };
  const img = await loadImage(file);
  if (isPng(file.name) || file.type === 'image/png') {
    // Keep transparency for overlays. Only shrink PNGs that are bigger than the screen.
    const fits = img.naturalWidth <= screen.width && img.naturalHeight <= screen.height;
    if (fits) return { blob: file, name: safeName(file.name, '.png') };
    return { blob: await canvasToPng(drawToScreen(img, 'fit', null)), name: safeName(file.name, '.png') };
  }
  return { blob: canvasToGrayBmp(drawToScreen(img, fitMode, '#fff')), name: safeName(file.name, '.bmp') };
}

function uploadBlob(blob, name, onProgress) {
  return new Promise((resolve, reject) => {
    const form = new FormData();
    form.append('file', blob, name);
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/upload?path=' + encodeURIComponent(info.folder), true);
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) onProgress(e.loaded / e.total);
    };
    xhr.onload = () => (xhr.status === 200 ? resolve() : reject(new Error(xhr.responseText || 'Upload failed')));
    xhr.onerror = () => reject(new Error('Connection lost'));
    xhr.send(form);
  });
}

async function ensureFolder() {
  if (info.folderExists) return;
  const slash = info.folder.lastIndexOf('/');
  const parent = slash <= 0 ? '/' : info.folder.substring(0, slash);
  const form = new FormData();
  form.append('name', info.folder.substring(slash + 1));
  form.append('path', parent);
  const response = await fetch('/mkdir', { method: 'POST', body: form });
  if (!response.ok) {
    const text = await response.text();
    if (!/already exists/i.test(text)) throw new Error('Could not create ' + info.folder + ': ' + text);
  }
  info.folderExists = true;
}

async function uploadFiles(files) {
  const list = document.getElementById('uploadList');
  const rows = files.map((file) => {
    const row = el('div', 'qz-up');
    const state = el('div', 'qz-up-state', 'Waiting');
    const track = el('div', 'qz-up-track');
    const fill = el('div', 'qz-up-fill');
    track.appendChild(fill);
    row.append(el('div', 'qz-up-name', file.name), state, track);
    list.appendChild(row);
    return { file, state, fill };
  });

  try {
    await ensureFolder();
  } catch (error) {
    for (const row of rows) {
      row.state.textContent = error.message;
      row.state.classList.add('error');
    }
    return;
  }

  let uploaded = 0;
  for (const row of rows) {
    try {
      row.state.textContent = 'Preparing';
      const { blob, name } = await prepare(row.file);
      row.state.textContent = 'Uploading';
      await uploadBlob(blob, name, (fraction) => {
        row.fill.style.width = Math.round(fraction * 100) + '%';
      });
      row.fill.style.width = '100%';
      existingNames.add(name.toLowerCase());
      row.state.textContent = 'Done · ' + name;
      uploaded += 1;
    } catch (error) {
      row.state.textContent = error.message;
      row.state.classList.add('error');
    }
  }
  if (uploaded > 0) await refresh();
}

// ---- Wiring ----

async function loadScreenSize() {
  try {
    const response = await fetch('/api/status');
    const status = await response.json();
    if (status.device === 'X3') screen = { width: 528, height: 792 };
  } catch (error) {
    // Keep the X4 / X4 Pro size.
  }
  document.getElementById('sizeHint').textContent =
    'Photos are resized to ' + screen.width + ' × ' + screen.height +
    ' and saved as grayscale BMPs. PNGs keep their transparency, for overlays.';
}

window.onload = () => {
  for (const button of document.querySelectorAll('#modeSeg button')) {
    button.onclick = () => setMode(Number(button.dataset.mode));
  }
  for (const button of document.querySelectorAll('#fitSeg button')) {
    button.onclick = () => {
      fitMode = button.dataset.fit;
      for (const other of document.querySelectorAll('#fitSeg button')) other.classList.toggle('active', other === button);
    };
  }
  document.getElementById('randomBtn').onclick = () => pin('');

  const input = document.getElementById('fileInput');
  input.onchange = () => {
    const files = Array.from(input.files || []);
    input.value = '';
    if (files.length) uploadFiles(files);
  };
  const drop = document.getElementById('dropZone');
  drop.addEventListener('dragover', (e) => {
    e.preventDefault();
    drop.classList.add('dragging');
  });
  drop.addEventListener('dragleave', () => drop.classList.remove('dragging'));
  drop.addEventListener('drop', (e) => {
    e.preventDefault();
    drop.classList.remove('dragging');
    const files = Array.from(e.dataTransfer.files || []).filter((f) => f.type.startsWith('image/') || isBmp(f.name));
    if (files.length) uploadFiles(files);
  });

  loadScreenSize();
  refresh();
};
