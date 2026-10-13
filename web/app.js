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

// fromTextArea initializes with the textarea's value; make sure it's empty.
editor.setValue('');

function getCode() { return editor.getValue(); }
function setCode(v) { editor.setValue(v || ''); }

editor.on('change', () => {
    if (debounceTimer) clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => parse(getCode()), 700);
});

// ---- Tab switching ----
function setTab(tab) {
    currentTab = tab;
    tabAst.classList.toggle('active', tab === 'ast');
    tabCfg.classList.toggle('active', tab === 'cfg');
    if (currentData) render(currentData);
}
tabAst.addEventListener('click', () => setTab('ast'));
tabCfg.addEventListener('click', () => setTab('cfg'));

// ---- Safe placeholders ----
function setPlaceholder(message) {
    svgBox.textContent = '';
    const div = document.createElement('div');
    div.className = 'placeholder';
    div.textContent = message;
    svgBox.appendChild(div);
}

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
    if (data.error) {
        statusEl.className = 'err';
        statusEl.textContent = 'Error: ' + data.error;
        setPlaceholder(data.error);
        diagEl.hidden = true;
        jsonBox.textContent = '';
        return;
    }

    const isCfg = currentTab === 'cfg';
    const svg = isCfg ? (data.cfg_svg || '') : (data.svg || '');
    svgBox.innerHTML = svg;
    if (!svg) setPlaceholder(isCfg ? 'No CFG picture produced.' : 'No AST picture produced.');
    viewTitle.textContent = isCfg
        ? 'CFG picture (basic blocks, hover for details)'
        : 'AST picture (hover nodes for details)';

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

    if (data.diagnostics && data.diagnostics.length) {
        diagEl.hidden = false;
        diagEl.textContent = data.diagnostics.join('\n');
    } else {
        diagEl.hidden = true;
    }

    jsonSummary.textContent = isCfg ? 'Raw CFG (JSON)' : 'Raw AST (JSON)';
    jsonBox.textContent = JSON.stringify(isCfg ? data.cfg : data.ast, null, 2);
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