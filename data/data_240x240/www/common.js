document.addEventListener('DOMContentLoaded', () => {

    fetch("header.html")
        .then(response => response.text())
        .then(html => {
            document.getElementById("header-container").innerHTML = html;
        })
        .catch(error => console.error("Error loading header:", error));

    document.getElementById('btnReboot')?.addEventListener('click', async () => {
        fetch('/reboot', { method: 'POST' })
            .then(response => {
                if (response.ok) {
                    alert('Reboot initiated');
                } else {
                    alert('Failed to initiate reboot');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('An error occurred');
            });
    });

    const hamburger = document.getElementById('hamburger');
    const menu = document.getElementById('menu');

    hamburger?.addEventListener('click', (e) => {
        e.stopPropagation();
        menu?.classList.toggle('active');
        hamburger.classList.toggle('active');
    });

    document.addEventListener('click', (e) => {
        if (menu?.classList.contains('active') && !e.target.closest('nav')) {
            menu.classList.remove('active');
            hamburger.classList.remove('active');
        }
    });
});
