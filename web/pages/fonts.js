let ttfSupported = false;
function formatSize(bytes) {
      if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
      if (bytes >= 1024) return (bytes / 1024).toFixed(0) + ' KB';
      return bytes + ' B';
    }

    async function loadFonts() {
      const el = document.getElementById('families');
      try {
        const res = await fetch('/api/fonts');
        const data = await res.json();
        ttfSupported = data.ttfSupported === true;
        // Build rows with DOM APIs and textContent so on-device family names
        // (which can contain arbitrary characters) cannot break markup or
        // execute script via innerHTML / inline onclick interpolation.
        el.replaceChildren();
        if (!data.families || data.families.length === 0) {
          const p = document.createElement('p');
          p.className = 'empty';
          p.textContent = 'No fonts installed';
          el.appendChild(p);
          return;
        }
        for (const f of data.families) {
          const row = document.createElement('div');
          row.className = 'family';

          const info = document.createElement('div');
          info.className = 'family-info';
          const h3 = document.createElement('h3');
          h3.textContent = f.name;
          info.appendChild(h3);
          const meta = document.createElement('span');
          meta.className = 'family-meta';
          const sizes = (f.sizes || []).join(', ');
          const filesSizes = (f.files || []).map(fi => formatSize(fi.size)).join(' + ');
          meta.textContent = sizes + 'pt · ' + filesSizes;
          info.appendChild(meta);

          const btn = document.createElement('button');
          btn.className = 'btn btn-danger';
          btn.textContent = 'Delete';
          // Capture name in the closure rather than interpolating into onclick.
          const familyName = f.name;
          btn.addEventListener('click', () => deleteFamily(familyName));

          row.appendChild(info);
          row.appendChild(btn);
          el.appendChild(row);
        }
      } catch (e) {
        el.replaceChildren();
        const p = document.createElement('p');
        p.className = 'empty';
        p.textContent = 'Failed to load font list';
        el.appendChild(p);
      }
    }

    async function deleteFamily(name) {
      if (!confirm('Delete font family "' + name + '"?')) return;
      const status = document.getElementById('status');
      status.className = '';
      status.style.display = 'block';
      status.textContent = 'Deleting ' + name + '...';
      try {
        const res = await fetch('/api/fonts/delete', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({family: name})
        });
        if (res.ok) {
          status.className = 'status-ok';
          status.textContent = 'Deleted "' + name + '".';
        } else {
          status.className = 'status-err';
          status.textContent = 'Failed to delete "' + name + '".';
        }
      } catch (err) {
        status.className = 'status-err';
        status.textContent = 'Delete error: ' + err.message;
      }
      await loadFonts();
    }

    // Derive family name from a .cpfont filename: take everything before the
    // last '-' or '_' (that separator precedes the size suffix, e.g. Bookerly_12.cpfont).
    function familyFromFilename(name) {
      if (/\.ttf$/i.test(name)) return name.replace(/\.ttf$/i, '').replace(/[-_ ]?(Regular|BoldItalic|Bold|Italic)$/i, '');
      const stem = name.replace(/\.cpfont$/i, '');
      const cut = Math.max(stem.lastIndexOf('-'), stem.lastIndexOf('_'));
      return cut > 0 ? stem.slice(0, cut) : stem;
    }

    // Sanitize to match firmware's [A-Za-z0-9_-]+ pattern.
    function sanitizeFamily(raw) {
      return raw.replace(/[^A-Za-z0-9_-]/g, '_');
    }

    function cpfontFilesOnly(fileList) {
      return Array.from(fileList).filter(f => (/\.cpfont$/i.test(f.name) || (ttfSupported && /\.ttf$/i.test(f.name))));
    }

    // QuartzInk: a picked folder may hold one family's files, or a folder per
    // family (e.g. fonts/NV Charis/...). Files two or more folders deep take
    // their family from the folder they sit in; otherwise from the filename.
    function familyForFile(file) {
      const parts = (file.webkitRelativePath || '').split('/');
      parts.pop();
      if (parts.length >= 2) return sanitizeFamily(parts[parts.length - 1]);
      return sanitizeFamily(familyFromFilename(file.name));
    }

    function groupByFamily(files) {
      const groups = new Map();
      for (const file of files) {
        const family = familyForFile(file);
        if (!groups.has(family)) groups.set(family, []);
        groups.get(family).push(file);
      }
      return groups;
    }

    document.getElementById('fontFiles').addEventListener('change', function() {
      const info = document.getElementById('pickedInfo');
      const files = cpfontFilesOnly(this.files);
      if (files.length === 0) {
        info.textContent = 'No .cpfont or .ttf files found in the selected folder.';
        return;
      }
      const families = [...groupByFamily(files).keys()];
      info.textContent = files.length + ' file' + (files.length === 1 ? '' : 's') + ' → ' +
        (families.length === 1 ? 'family "' + families[0] + '"'
                               : families.length + ' families: ' + families.join(', '));
    });

    document.getElementById('uploadForm').addEventListener('submit', async function(e) {
      e.preventDefault();
      const status = document.getElementById('status');
      const files = cpfontFilesOnly(document.getElementById('fontFiles').files);
      if (files.length === 0) {
        status.className = 'status-err';
        status.style.display = 'block';
        status.textContent = 'No .cpfont or .ttf files selected.';
        return;
      }

      const groups = groupByFamily(files);
      status.className = '';
      status.style.display = 'block';

      let uploaded = 0;
      const failed = [];
      for (const [family, familyFiles] of groups) {
        for (const file of familyFiles) {
          status.textContent = 'Uploading ' + (uploaded + failed.length + 1) + '/' + files.length + ': ' +
            family + ' / ' + file.name;
          const formData = new FormData();
          formData.append('family', family);
          formData.append('file', file, file.name);
          try {
            const res = await fetch('/api/fonts/upload', { method: 'POST', body: formData });
            const data = await res.json();
            if (!data.ok) {
              failed.push(family + ' / ' + file.name + ' (' + (data.error || 'unknown error') + ')');
              continue;
            }
          } catch (err) {
            failed.push(family + ' / ' + file.name + ' (' + err.message + ')');
            continue;
          }
          uploaded++;
        }
      }

      const familyText = groups.size === 1 ? 'family "' + [...groups.keys()][0] + '"' : groups.size + ' families';
      if (failed.length === 0) {
        status.className = 'status-ok';
        status.textContent = 'Uploaded ' + uploaded + ' file' + (uploaded === 1 ? '' : 's') + ' to ' + familyText + '.';
      } else {
        status.className = 'status-err';
        status.textContent = 'Uploaded ' + uploaded + ' of ' + files.length + ' files to ' + familyText +
          '. Failed: ' + failed.join('; ');
      }
      await loadFonts();
    });

    loadFonts();
