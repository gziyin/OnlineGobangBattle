/**
 * 五境修行 — 大厅壁纸 manifest 与 picker 逻辑
 */

export const REALM_WALLPAPERS = [
    { id: 'rain', file: 'assets/1雨天棋.png', name: '雨天棋', stage: '入困', tagline: '残局复盘，心境低谷' },
    { id: 'ascend', file: 'assets/2与天齐.png', name: '与天齐', stage: '破境', tagline: '顿悟踏空，自我超越' },
    { id: 'heaven', file: 'assets/3与天棋.png', name: '与天棋', stage: '齐天', tagline: '与天道对弈，齐观宇宙' },
    { id: 'guard', file: 'assets/4御天敌.png', name: '御天敌', stage: '护道', tagline: '与天并肩，抵御域外' },
    { id: 'unity', file: 'assets/5天地棋.png', name: '天地棋', stage: '合一', tagline: '化天地为盘，归于和谐' },
];

const STORAGE_KEY = 'gobang_hall_wallpaper';
const DEFAULT_ID = 'rain';

function findRealm(id) {
    return REALM_WALLPAPERS.find((r) => r.id === id) || REALM_WALLPAPERS[0];
}

export function getStoredWallpaperId() {
    const stored = localStorage.getItem(STORAGE_KEY);
    return findRealm(stored).id;
}

export function applyWallpaper(id) {
    const realm = findRealm(id);
    const wallpaperEl = document.getElementById('storyWallpaper');
    const inkScene = document.getElementById('inkScene');
    const triggerLabel = document.querySelector('.realm-picker__trigger-label');

    if (wallpaperEl) {
        wallpaperEl.style.backgroundImage = `url('${realm.file}')`;
        wallpaperEl.classList.add('is-active');
    }
    if (inkScene) {
        inkScene.classList.add('is-hidden');
    }
    if (triggerLabel) {
        triggerLabel.textContent = `${realm.stage} · ${realm.name}`;
    }

    document.querySelectorAll('.realm-card').forEach((card) => {
        card.classList.toggle('is-active', card.dataset.realmId === realm.id);
    });

    localStorage.setItem(STORAGE_KEY, realm.id);
    return realm.id;
}

function closePickerPanel() {
    const panel = document.getElementById('realmPickerPanel');
    const trigger = document.getElementById('realmPickerTrigger');
    if (panel) {
        panel.hidden = true;
    }
    if (trigger) {
        trigger.setAttribute('aria-expanded', 'false');
    }
}

function openPickerPanel() {
    const panel = document.getElementById('realmPickerPanel');
    const trigger = document.getElementById('realmPickerTrigger');
    if (panel) {
        panel.hidden = false;
    }
    if (trigger) {
        trigger.setAttribute('aria-expanded', 'true');
    }
}

function buildRealmCards(container) {
    container.innerHTML = '';
    REALM_WALLPAPERS.forEach((realm) => {
        const card = document.createElement('button');
        card.type = 'button';
        card.className = 'realm-card';
        card.dataset.realmId = realm.id;
        card.setAttribute('aria-label', `${realm.stage} ${realm.name}：${realm.tagline}`);
        card.innerHTML = `
            <span class="realm-card__thumb" style="background-image: url('${realm.file}')"></span>
            <span class="realm-card__stage">${realm.stage}</span>
            <span class="realm-card__name">${realm.name}</span>
            <span class="realm-card__tagline">${realm.tagline}</span>
        `;
        card.addEventListener('click', () => {
            applyWallpaper(realm.id);
            closePickerPanel();
        });
        container.appendChild(card);
    });
}

export function initWallpaperPicker() {
    if (!sessionStorage.getItem('gobang_token')) {
        return;
    }

    const panel = document.getElementById('realmPickerPanel');
    const trigger = document.getElementById('realmPickerTrigger');
    const track = document.getElementById('realmPickerTrack');

    if (!panel || !trigger || !track) {
        return;
    }

    buildRealmCards(track);
    applyWallpaper(getStoredWallpaperId());

    trigger.addEventListener('click', (e) => {
        e.stopPropagation();
        if (panel.hidden) {
            openPickerPanel();
        } else {
            closePickerPanel();
        }
    });

    document.addEventListener('click', (e) => {
        if (!e.target.closest('.realm-picker')) {
            closePickerPanel();
        }
    });

    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            closePickerPanel();
        }
    });
}
