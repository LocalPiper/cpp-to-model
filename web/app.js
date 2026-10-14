// ---- DOM refs ----
const parseBtn     = document.getElementById('parseBtn');
const statusEl     = document.getElementById('status');
const svgBox       = document.getElementById('svgBox');
const diagEl       = document.getElementById('diagnostics');
const jsonBox      = document.getElementById('jsonBox');
const tabAst       = document.getElementById('tabAst');
const tabCfg       = document.getElementById('tabCfg');
const viewTitle    = document.getElementById('viewTitle');
const jsonSummary  = document.getElementById('jsonSummary');

const samplesBtn      = document.getElementById('samplesBtn');
const samplesDropdown = document.getElementById('samplesDropdown');
const samplesList     = document.getElementById('samplesList');
const manageSamplesBtn= document.getElementById('manageSamplesBtn');
const saveSampleBtn   = document.getElementById('saveSampleBtn');

const saveModal  = document.getElementById('saveModal');
const saveName   = document.getElementById('saveName');
const saveDesc   = document.getElementById('saveDesc');
const saveCancel = document.getElementById('saveCancel');
const saveConfirm= document.getElementById('saveConfirm');
const saveError  = document.getElementById('saveError');

let currentData = null;
let currentTab  = 'ast';
let debounceTimer = null;

// ---- Real editor: CodeMirror ----
// Move the existing textarea into a wrapper, then let CodeMirror take it over
// via fromTextArea(). CodeMirror hides the textarea and renders its own DOM in
// the same place — only one visible input.
const ta = document.getElementById('codeInput');
const editorWrap = document.createElement('div');
editorWrap.className = 'editor-wrap';
ta.parentNode.insertBefore(editorWrap, ta);
editorWrap.appendChild(ta);

const editor = CodeMirror.fromTextArea(ta, {
    mode: 'text/x-c++src',
    theme: 'material-darker',
    lineNumbers: true,
    indentUnit: 4,
    tabSize: 4,
    indentWithTabs: false,
    smartIndent: true,
    electricChars: true,
    matchBrackets: true,
    autoCloseBrackets: true,
    foldGutter: true,
    gutters: ['CodeMirror-linenumbers', 'CodeMirror-foldgutter'],
    styleActiveLine: true,
    lineWrapping: false,
    extraKeys: {
        'Ctrl-/': 'toggleComment',
        'Cmd-/':  'toggleComment',
        'Tab': (cm) => cm.somethingSelected()
            ? cm.indentSelection('add')
            : cm.replaceSelection('    ', 'end'),
        'Shift-Tab': (cm) => cm.indentSelection('subtract'),
    }
});
editor.setSize('100%', '100%');

// fromTextArea initializes with the textarea's value; make sure it's empty.
editor.setValue('');

function getCode() { return editor.getValue(); }
function setCode(v) { editor.setValue(v || ''); }

editor.on('change', () => {
    if (debounceTimer) clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => parse(getCode()), 700);
});

// ---- Tab switching ----
const tabSsa = document.getElementById('tabSsa');
const tabWhy = document.getElementById('tabWhy');

function setTab(tab) {
    currentTab = tab;
    tabAst.classList.toggle('active', tab === 'ast');
    tabCfg.classList.toggle('active', tab === 'cfg');
    tabSsa.classList.toggle('active', tab === 'ssa');
    tabWhy.classList.toggle('active', tab === 'why');
    if (currentData) render(currentData);
}
tabAst.addEventListener('click', () => setTab('ast'));
tabCfg.addEventListener('click', () => setTab('cfg'));
tabSsa.addEventListener('click', () => setTab('ssa'));
tabWhy.addEventListener('click', () => setTab('why'));

// ---- Safe placeholders ----
function setPlaceholder(message) {
    svgBox.textContent = '';
    const div = document.createElement('div');
    div.className = 'placeholder';
    div.textContent = message;
    svgBox.appendChild(div);
}

// ---- Drag-to-pan for the SVG/text viewer ----
// We pan by mutating scrollLeft/scrollTop, so it works whether or not the
// content overflows. Wheel-zoom is intentionally not added (out of scope).
(function installPan() {
    let panning = false;
    let startX = 0, startY = 0, startLeft = 0, startTop = 0;

    svgBox.addEventListener('mousedown', (e) => {
        // Left button only; ignore clicks on interactive elements inside SVG
        // (hover titles use <title>, which isn't interactive, so this is fine).
        if (e.button !== 0) return;
        panning = true;
        svgBox.classList.add('panning');
        startX = e.clientX;
        startY = e.clientY;
        startLeft = svgBox.scrollLeft;
        startTop  = svgBox.scrollTop;
        e.preventDefault();
    });

    window.addEventListener('mousemove', (e) => {
        if (!panning) return;
        svgBox.scrollLeft = startLeft - (e.clientX - startX);
        svgBox.scrollTop  = startTop  - (e.clientY - startY);
    });

    window.addEventListener('mouseup', () => {
        if (!panning) return;
        panning = false;
        svgBox.classList.remove('panning');
    });

    // Touch: same idea, one finger.
    svgBox.addEventListener('touchstart', (e) => {
        if (e.touches.length !== 1) return;
        panning = true;
        startX = e.touches[0].clientX;
        startY = e.touches[0].clientY;
        startLeft = svgBox.scrollLeft;
        startTop  = svgBox.scrollTop;
    }, { passive: true });

    svgBox.addEventListener('touchmove', (e) => {
        if (!panning || e.touches.length !== 1) return;
        svgBox.scrollLeft = startLeft - (e.touches[0].clientX - startX);
        svgBox.scrollTop  = startTop  - (e.touches[0].clientY - startY);
    }, { passive: true });

    svgBox.addEventListener('touchend', () => { panning = false; });
})();

// ---- JSON fetch helper ----
async function fetchJson(url, options) {
    const res = await fetch(url, options);
    const text = await res.text();
    let body = null;
    try { body = text ? JSON.parse(text) : null; }
    catch (e) {
        throw new Error('Server returned non-JSON (' + res.status + '): ' + text.slice(0, 200));
    }
    if (!res.ok) throw new Error((body && body.error) || ('HTTP ' + res.status));
    return body;
}

// ---- Parse + render ----
async function parse(text) {
    statusEl.className = 'busy';
    statusEl.textContent = 'Parsing in C++ (clang)...';
    try {
        const data = await fetchJson('/process', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ text })
        });
        render(data);
    } catch (err) {
        statusEl.className = 'err';
        statusEl.textContent = 'Request failed: ' + err.message;
        setPlaceholder(err.message);
    }
}

function render(data) {
    currentData = data;

    // The WHY tab is special: it can render even when the C++ processor
    // returned an error, as long as we have some textual "why" payload.
    if (data.error && currentTab !== 'why') {
        statusEl.className = 'err';
        statusEl.textContent = 'Error: ' + data.error;
        setPlaceholder(data.error);
        diagEl.hidden = true;
        jsonBox.textContent = '';
        return;
    }

    const isCfg = currentTab === 'cfg';
    const isSsa = currentTab === 'ssa';
    const isWhy = currentTab === 'why';

    // --- Pick content ---
    let svg = '';
    if (isCfg)      svg = data.cfg_svg || '';
    else if (isSsa) svg = data.ssa_svg || '';
    else if (!isWhy) svg = data.svg || '';

    if (isWhy) {
        renderWhy(data);
    } else {
        svgBox.innerHTML = svg;
        if (!svg) {
            const label = isCfg ? 'CFG' : isSsa ? 'SSA' : 'AST';
            setPlaceholder('No ' + label + ' picture produced.');
        }
    }

    // --- View title ---
    viewTitle.textContent =
        isCfg ? 'CFG picture (basic blocks, hover for details)'
      : isSsa ? 'SSA picture (placeholder — coming soon)'
      : isWhy ? 'WHY explanation (text)'
      :         'AST picture (hover nodes for details)';

    // --- Status line ---
    if (!data.error) {
        let st = '\u2713 ' + data.node_count + ' node(s), ' +
                 data.errors + ' error(s), ' + data.warnings + ' warning(s)';
        if (data.truncated) st += ' \u00b7 truncated';
        if (isCfg && data.cfg && Array.isArray(data.cfg.blocks)) {
            let bbs = 0, conds = 0;
            for (const blk of data.cfg.blocks) {
                if (blk.kind === 'bb') bbs++;
                else if (blk.kind === 'condition') conds++;
            }
            st += ' \u00b7 ' + bbs + ' basic block(s)' +
                  (conds ? ', ' + conds + ' condition(s)' : '');
        }
        statusEl.className = data.errors ? 'err' : 'ok';
        statusEl.textContent = st;
    }

    // --- Diagnostics ---
    if (data.diagnostics && data.diagnostics.length) {
        diagEl.hidden = false;
        diagEl.textContent = data.diagnostics.join('\n');
    } else {
        diagEl.hidden = true;
    }

    // --- Raw JSON panel ---
    if (isWhy) {
        jsonSummary.textContent = 'Raw WHY payload (JSON)';
        jsonBox.textContent = data.why ? JSON.stringify(data.why, null, 2) : '(no why payload)';
    } else {
        jsonSummary.textContent = isCfg ? 'Raw CFG (JSON)'
                              : isSsa ? 'Raw SSA (JSON)'
                              :         'Raw AST (JSON)';
        const payload = isCfg ? data.cfg : isSsa ? data.ssa : data.ast;
        jsonBox.textContent = JSON.stringify(payload, null, 2);
    }
}

function renderWhy(data) {
    svgBox.innerHTML = '';
    const pre = document.createElement('div');
    pre.className = 'why-text';
    // Accept a few plausible shapes from the server.
    const why = data.why;
    let text = '';
    if (typeof why === 'string') {
        text = why;
    } else if (why && typeof why === 'object') {
        if (typeof why.text === 'string') text = why.text;
        else text = JSON.stringify(why, null, 2);
    } else if (Array.isArray(data.why_lines)) {
        text = data.why_lines.join('\n');
    }
    if (!text) text = 'No WHY explanation available for this input.';
    pre.textContent = text;
    svgBox.appendChild(pre);
}

parseBtn.addEventListener('click', () => {
    if (debounceTimer) clearTimeout(debounceTimer);
    parse(getCode());
});

// ---- Sample library ----
async function refreshSamplesList() {
    try {
        const data = await fetchJson('/samples');
        const samples = (data && data.samples) || [];
        samplesList.textContent = '';
        if (!samples.length) {
            const empty = document.createElement('div');
            empty.className = 'dropdown-empty';
            empty.textContent = 'No samples yet.';
            samplesList.appendChild(empty);
            return;
        }
        for (const s of samples) {
            const btn = document.createElement('button');
            btn.className = 'dropdown-item';
            btn.type = 'button';
            const nameEl = document.createElement('div');
            nameEl.textContent = s.name;
            btn.appendChild(nameEl);
            if (s.description) {
                const desc = document.createElement('span');
                desc.className = 'desc';
                desc.textContent = s.description;
                btn.appendChild(desc);
            }
            btn.addEventListener('click', () => loadSampleById(s.id));
            samplesList.appendChild(btn);
        }
    } catch (err) {
        samplesList.textContent = '';
        const empty = document.createElement('div');
        empty.className = 'dropdown-empty';
        empty.textContent = 'Failed to load samples: ' + err.message;
        samplesList.appendChild(empty);
    }
}

async function loadSampleById(id) {
    try {
        const data = await fetchJson('/samples/' + encodeURIComponent(id));
        setCode(data.code || '');
        closeDropdown();
        if (debounceTimer) clearTimeout(debounceTimer);
        parse(getCode());
    } catch (err) {
        statusEl.className = 'err';
        statusEl.textContent = 'Failed to load sample: ' + err.message;
    }
}

function openDropdown() {
    samplesDropdown.hidden = false;
    refreshSamplesList();
}
function closeDropdown() { samplesDropdown.hidden = true; }

samplesBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    if (samplesDropdown.hidden) openDropdown();
    else closeDropdown();
});

document.addEventListener('click', (e) => {
    if (!samplesDropdown.hidden && !samplesDropdown.contains(e.target) && e.target !== samplesBtn) {
        closeDropdown();
    }
});

manageSamplesBtn.addEventListener('click', () => {
    closeDropdown();
    alert('Samples are stored as JSON files in the "samples/" directory next to index.html.');
});

// ---- Save sample modal ----
function openSaveModal() {
    saveError.hidden = true;
    saveError.textContent = '';
    saveName.value = '';
    saveDesc.value = '';
    saveModal.hidden = false;
    saveName.focus();
}
function closeSaveModal() {
    saveModal.hidden = true;
    saveConfirm.disabled = false;
    saveConfirm.textContent = 'Save';
}

saveSampleBtn.addEventListener('click', openSaveModal);
saveCancel.addEventListener('click', (e) => {
    e.preventDefault();
    e.stopPropagation();
    closeSaveModal();
});
document.querySelector('#saveModal .modal-backdrop').addEventListener('click', closeSaveModal);
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && !saveModal.hidden) closeSaveModal();
});

saveConfirm.addEventListener('click', async (e) => {
    e.preventDefault();
    e.stopPropagation();
    const name = saveName.value.trim();
    const description = saveDesc.value.trim();
    const code = getCode();
    if (!name) {
        saveError.hidden = false;
        saveError.textContent = 'Please enter a name.';
        return;
    }
    saveConfirm.disabled = true;
    saveConfirm.textContent = 'Saving\u2026';
    try {
        const data = await fetchJson('/samples', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, description, code })
        });
        closeSaveModal();
        statusEl.className = 'ok';
        statusEl.textContent = 'Saved sample: ' + (data.sample ? data.sample.name : name);
        refreshSamplesList();
    } catch (err) {
        saveError.hidden = false;
        saveError.textContent = 'Save failed: ' + err.message;
        saveConfirm.disabled = false;
        saveConfirm.textContent = 'Save';
    }
});

// ---- First-visit seeding + boot ----
const DEFAULT_SAMPLE_CODE = `#include <iostream>
#include <vector>

struct Point {
    int x;
    int y;
};

int square(int n) {
    return n * n;
}

int main() {
    std::vector<Point> points;
    for (int i = 0; i < 3; ++i) {
        points.push_back({i, i * i});
    }
    int total = 0;
    for (const Point& p : points) {
        total += square(p.x) + p.y;
    }
    std::cout << "total: " << total << std::endl;
    return 0;
}`;

async function ensureDefaultSample() {
    try {
        const data = await fetchJson('/samples');
        if (data && data.samples && data.samples.length === 0) {
            await fetchJson('/samples', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    name: 'Default (STL + loops)',
                    description: 'Structs, range-for, vector, iostream.',
                    code: DEFAULT_SAMPLE_CODE
                })
            });
        }
    } catch (err) {
        statusEl.className = 'err';
        statusEl.textContent = 'Sample library unavailable: ' + err.message;
    }
}

(async () => {
    setCode('');
    await ensureDefaultSample();
    try {
        const data = await fetchJson('/samples');
        if (data && data.samples && data.samples.length) {
            await loadSampleById(data.samples[0].id);
        }
    } catch (_) { /* already reported */ }
})();