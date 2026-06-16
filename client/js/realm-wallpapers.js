/**
 * 五境修行 — 大厅壁纸 manifest 与 picker 逻辑
 */

export const AMBIENT_BG = 'assets/gobang-ambient-bg.jpeg';

export const REALM_WALLPAPERS = [
    { id: 'rain', file: 'assets/1雨天棋.png', name: '雨天棋', stage: '入困', tagline: '残局复盘，心境低谷' },
    { id: 'ascend', file: 'assets/2与天齐.png', name: '与天齐', stage: '破境', tagline: '顿悟踏空，自我超越' },
    { id: 'heaven', file: 'assets/3与天棋.png', name: '与天棋', stage: '齐天', tagline: '与天道对弈，齐观宇宙' },
    { id: 'guard', file: 'assets/4御天敌.png', name: '御天敌', stage: '护道', tagline: '与天并肩，抵御域外' },
    { id: 'unity', file: 'assets/5天地棋.png', name: '天地棋', stage: '合一', tagline: '化天地为盘，归于和谐' },
];

const STORAGE_KEY = 'gobang_hall_wallpaper';
const WALLPAPER_ASPECT = 1024 / 1536;

function findRealm(id) {
    return REALM_WALLPAPERS.find((r) => r.id === id) || null;
}

export function updateWallpaperEdges() {
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    const viewRatio = vw / vh;
    let widthPct = 100;
    if (viewRatio > WALLPAPER_ASPECT) {
        widthPct = (WALLPAPER_ASPECT / viewRatio) * 100;
    }
    const inset = (100 - widthPct) / 2;
    document.documentElement.style.setProperty('--wallpaper-edge-left', `${inset}%`);
    document.documentElement.style.setProperty('--wallpaper-edge-right', `${100 - inset}%`);
}

function clearWallpaperEdges() {
    document.documentElement.style.removeProperty('--wallpaper-edge-left');
    document.documentElement.style.removeProperty('--wallpaper-edge-right');
}

export function getStoredWallpaperId() {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (!stored || stored === 'none') {
        return null;
    }
    const realm = findRealm(stored);
    return realm ? realm.id : null;
}

export function clearWallpaper() {
    const wallpaperEl = document.getElementById('storyWallpaper');
    const triggerLabel = document.querySelector('.realm-picker__trigger-label');

    if (wallpaperEl) {
        wallpaperEl.style.backgroundImage = '';
        wallpaperEl.classList.remove('is-active');
    }
    document.body.classList.remove('has-wallpaper');
    clearWallpaperEdges();

    if (triggerLabel) {
        triggerLabel.textContent = '五境 · 换壁纸';
    }

    document.querySelectorAll('.realm-card').forEach((card) => {
        card.classList.toggle('is-active', card.dataset.realmId === 'none');
    });
}

export function applyWallpaper(id) {
    if (id === 'none' || id === null) {
        clearWallpaper();
        localStorage.setItem(STORAGE_KEY, 'none');
        return 'none';
    }

    const realm = findRealm(id);
    if (!realm) {
        clearWallpaper();
        localStorage.setItem(STORAGE_KEY, 'none');
        return 'none';
    }

    const wallpaperEl = document.getElementById('storyWallpaper');
    const triggerLabel = document.querySelector('.realm-picker__trigger-label');

    if (wallpaperEl) {
        wallpaperEl.style.backgroundImage = `url('${realm.file}')`;
        wallpaperEl.classList.add('is-active');
    }
    document.body.classList.add('has-wallpaper');
    updateWallpaperEdges();

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

    const defaultCard = document.createElement('button');
    defaultCard.type = 'button';
    defaultCard.className = 'realm-card';
    defaultCard.dataset.realmId = 'none';
    defaultCard.setAttribute('aria-label', '默认氛围：玄墨水墨底图');
    defaultCard.innerHTML = `
        <span class="realm-card__thumb realm-card__thumb--default" style="background-image: url('${AMBIENT_BG}')"></span>
        <span class="realm-card__stage">默认</span>
        <span class="realm-card__name">默认氛围</span>
        <span class="realm-card__tagline">玄墨底 · 水墨棋韵</span>
    `;
    defaultCard.addEventListener('click', () => {
        applyWallpaper('none');
        closePickerPanel();
    });
    container.appendChild(defaultCard);

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

let resizeListenerBound = false;

function bindWallpaperResize() {
    if (resizeListenerBound) {
        return;
    }
    resizeListenerBound = true;
    window.addEventListener('resize', () => {
        if (document.body.classList.contains('has-wallpaper')) {
            updateWallpaperEdges();
        }
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
    bindWallpaperResize();

    const storedId = getStoredWallpaperId();
    if (storedId) {
        applyWallpaper(storedId);
    } else {
        clearWallpaper();
    }

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
