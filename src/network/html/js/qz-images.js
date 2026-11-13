// QuartzInk: getting images ready for the e-reader's sleep screens, shared by
// the Wallpapers and Extras pages. Everything runs in the browser, so the
// reader only ever receives files sized for its own screen.
//
// Wallpapers become 8-bit grayscale BMPs at the screen's size (the reader
// dithers them). Overlays stay PNGs and keep their transparency; one drawn for
// a full screen of a similar shape is stretched to this screen exactly.
(function () {
  'use strict';

  function isPng(name) {
    return /\.png$/i.test(name);
  }

  function isBmp(name) {
    return /\.bmp$/i.test(name);
  }

  function loadImage(blob) {
    return new Promise((resolve, reject) => {
      const url = URL.createObjectURL(blob);
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

  // fit: 'fill' covers the screen (trims overflow), 'fit' shows the whole
  // image (bars), 'stretch' maps it onto the screen exactly.
  function drawToScreen(img, screen, fit, background) {
    const canvas = document.createElement('canvas');
    canvas.width = screen.width;
    canvas.height = screen.height;
    const ctx = canvas.getContext('2d');
    if (background) {
      ctx.fillStyle = background;
      ctx.fillRect(0, 0, canvas.width, canvas.height);
    }
    let w = canvas.width;
    let h = canvas.height;
    if (fit !== 'stretch') {
      const scale =
        fit === 'fit'
          ? Math.min(canvas.width / img.naturalWidth, canvas.height / img.naturalHeight)
          : Math.max(canvas.width / img.naturalWidth, canvas.height / img.naturalHeight);
      w = img.naturalWidth * scale;
      h = img.naturalHeight * scale;
    }
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

  // A name the card and the reader's fonts both handle, not already in `taken`
  // (a Set of lower-case names); collisions get " (2)", " (3)"...
  function safeName(name, extension, taken) {
    const stem =
      (name.replace(/\.[^.]*$/, '').replace(/[^A-Za-z0-9 _\-()]+/g, '_').trim() || 'wallpaper').substring(0, 60);
    let candidate = stem + extension;
    for (let n = 2; taken.has(candidate.toLowerCase()); n++) candidate = stem + ' (' + n + ')' + extension;
    return candidate;
  }

  // Readies one file for the reader. Returns { group, blob, name }, where group
  // is 'wallpapers' or 'overlays'.
  //   opts.screen: { width, height } of the reader (portrait)
  //   opts.fit:    'fill' or 'fit' for photos becoming wallpapers
  //   opts.taken:  { wallpapers: Set, overlays: Set } of names already used
  //   opts.kind:   'wallpaper' or 'overlay' to decide; otherwise PNGs are
  //                overlays and everything else a wallpaper
  async function prepare(file, opts) {
    const taken = opts.taken;
    const kind = opts.kind || (!isBmp(file.name) && (isPng(file.name) || file.type === 'image/png') ? 'overlay' : 'wallpaper');
    if (isBmp(file.name)) {
      // The reader scales BMPs itself.
      const group = kind === 'overlay' ? 'overlays' : 'wallpapers';
      return { group, blob: file, name: safeName(file.name, '.bmp', taken[group]) };
    }
    const img = await loadImage(file);
    if (kind === 'overlay') {
      const name = safeName(file.name, '.png', taken.overlays);
      // Keep transparency. An overlay drawn for a full screen (close to this
      // one's shape, like a 480x800 frame on the X3's 528x792) is stretched to
      // exactly this screen: shrinking it to fit would bake empty strips into
      // the file. Exact-size PNGs go up unchanged; others shrink to fit.
      const { width: sw, height: sh } = opts.screen;
      const png = isPng(file.name) || file.type === 'image/png';
      if (png && img.naturalWidth === sw && img.naturalHeight === sh) return { group: 'overlays', blob: file, name };
      const shapeOff = Math.abs(img.naturalWidth / img.naturalHeight / (sw / sh) - 1);
      if (shapeOff <= 0.12) {
        return { group: 'overlays', blob: await canvasToPng(drawToScreen(img, opts.screen, 'stretch', null)), name };
      }
      const fits = img.naturalWidth <= sw && img.naturalHeight <= sh;
      if (png && fits) return { group: 'overlays', blob: file, name };
      return { group: 'overlays', blob: await canvasToPng(drawToScreen(img, opts.screen, 'fit', null)), name };
    }
    return {
      group: 'wallpapers',
      blob: canvasToGrayBmp(drawToScreen(img, opts.screen, opts.fit || 'fill', '#fff')),
      name: safeName(file.name, '.bmp', taken.wallpapers),
    };
  }

  // Uploads to a folder on the card through the regular /upload route.
  function uploadBlob(folder, blob, name, onProgress) {
    return new Promise((resolve, reject) => {
      const form = new FormData();
      form.append('file', blob, name);
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/upload?path=' + encodeURIComponent(folder), true);
      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable && onProgress) onProgress(e.loaded / e.total);
      };
      xhr.onload = () => (xhr.status === 200 ? resolve() : reject(new Error(xhr.responseText || 'Upload failed')));
      xhr.onerror = () => reject(new Error('Connection lost'));
      xhr.send(form);
    });
  }

  window.QzImages = { isPng, isBmp, loadImage, prepare, uploadBlob, safeName };
})();
