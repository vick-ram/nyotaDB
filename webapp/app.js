const API_URL = "http://localhost:8081/query";
const TABLE_NAME = "notes";

// On Page Load: Initialize Table and Fetch Data
document.addEventListener('DOMContentLoaded', async () => {
    // 1. Ensure table exists (CREATE TABLE)
    await runSQL(`CREATE TABLE ${TABLE_NAME} (id INT PRIMARY, title STRING(64), content STRING(255));`);
    // 2. Load existing notes
    fetchNotes();
});

// CREATE / UPDATE Logic
document.getElementById('noteForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const title = document.getElementById('noteTitle').value;
    const content = document.getElementById('noteContent').value;
    
    // Using a simple timestamp as a Primary Key for the B-Tree
    const id = Date.now() % 1000000; 

    // SQL INSERT Operation
    const sql = `INSERT INTO ${TABLE_NAME} VALUES (${id}, '${title}', '${content}');`;
    
    const result = await runSQL(sql);
    if (!result.error_message) {
        document.getElementById('noteForm').reset();
        fetchNotes();
    } else {
        alert("Error: " + result.error_message);
    }
});

// READ Logic
async function fetchNotes() {
    const result = await runSQL(`SELECT * FROM ${TABLE_NAME};`);
    const grid = document.getElementById('notesGrid');
    grid.innerHTML = '';

    if (result.rows && result.rows.length > 0) {
        result.rows.forEach(row => {
            const [id, title, content] = row;
            const card = document.createElement('div');
            card.className = 'note-card';
            card.innerHTML = `
                <h3>${title}</h3>
                <p>${content}</p>
                <div class="actions">
                    <button class="delete-btn" onclick="deleteNote(${id})">Delete Permanent</button>
                </div>
            `;
            grid.appendChild(card);
        });
    } else {
        grid.innerHTML = '<p>No notes found in the B-Tree.</p>';
    }
}

// DELETE Logic
async function deleteNote(id) {
    if (confirm("Are you sure you want to delete this from the database?")) {
        await runSQL(`DELETE FROM ${TABLE_NAME} WHERE id = ${id};`);
        fetchNotes();
    }
}

// Core API Wrapper
async function runSQL(queryText) {
    try {
        const response = await fetch(API_URL, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ sql: queryText })
        });
        const data = await response.json();
        document.getElementById('db-status').innerText = "Connected: B-Tree Stable";
        return data;
    } catch (err) {
        document.getElementById('db-status').innerText = "Database Offline";
        console.error("Database connection failed", err);
        return { error_message: "Could not reach DB server" };
    }
}