// A short device line under the tiles: model, firmware and address.
async function fetchStatus() {
  try {
    const response = await fetch('/api/status');
    if (!response.ok) throw new Error(response.status + ' ' + response.statusText);
    const data = await response.json();
    const parts = [data.device, data.version ? 'v' + data.version : '', data.ip].filter(Boolean);
    document.getElementById('deviceLine').textContent = parts.join(' · ');
  } catch (error) {
    console.error('Error fetching status:', error);
  }
}

window.onload = fetchStatus;
