/*
 * dmod in a browser - the page as dmodOS shows it, without dmodOS.
 *
 * On the device the page is a view shown by dmview and `phoneui` (the application) is on the other side of the channel: dmod.prop / get /
 * set / onchange / emit are dmview's properties and events (docs/contract.md), compiled into the view. The conversion does not run this
 * file. Here it plays a small part of the application - the toggles of the quick settings, the backlight, the clock, a toast for what is
 * not there yet - so that the page can be designed in a browser. The data of the lists is the page's own sample text.
 */
(function () {
    const props = {};
    const listeners = {};
    const QUICK = 9;
    const NOT_THERE = { search: 'Search: the keyboard comes next', send: 'Send: the keyboard comes next', compose: 'New message: not available yet',
                        call: 'Call: not available yet', banner: 'Store: not available yet' };

    function set(name, value) {
        if (props[name] === value) return;
        props[name] = value;
        (listeners[name] || []).forEach(fn => fn());
    }

    window.dmod = {
        prop(name, initial) { if (!(name in props)) props[name] = initial; return props[name]; },
        get(name) { return props[name]; },
        set(name, value) { props[name] = value; },
        onchange(name, fn) { (listeners[name] = listeners[name] || []).push(fn); },
        emit(name, value) {
            console.log('emit', name, value === undefined ? 0 : value);
            if (name === 'quick') set('q' + value, props['q' + value] ? 0 : 1);
            else if (name === 'level') set('q.level', value);
            else if (name === 'launch') set('toast', 'App ' + value + ': not available yet' + (Date.now() % 2 ? '' : ' '));
            else if (NOT_THERE[name]) set('toast', NOT_THERE[name] + (Date.now() % 2 ? '' : ' '));
        },
    };

    /* a clock that moves, as the application's does */
    window.addEventListener('load', () => {
        const tick = () => {
            const d = new Date();
            set('clock', String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0'));
        };
        tick();
        setInterval(tick, 10000);
    });
})();
