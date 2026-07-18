# Filament 4 Dashboard - Quick Start Guide

This repository now includes a complete admin dashboard built with Laravel 12 and Filament 4.

## 📁 Location

The dashboard is located in the `/dashboard` directory.

## 🚀 Quick Start

### 1. Navigate to Dashboard Directory
```bash
cd dashboard
```

### 2. Install Dependencies (if needed)
```bash
composer install
```

### 3. Set Up Environment
```bash
cp .env.example .env
php artisan key:generate
```

### 4. Create Database
```bash
touch database/database.sqlite
```

### 5. Run Migrations
```bash
php artisan migrate
```

### 6. Seed Sample Data
```bash
php artisan db:seed
```

This will create:
- 1 test user
- 5 categories (Technology, Business, Lifestyle, Travel, Food)
- 7 sample posts

### 7. Create Admin User
```bash
php artisan make:filament-user
```

Or use the pre-configured admin:
- **Email**: admin@example.com
- **Password**: password

### 8. Start Development Server
```bash
php artisan serve
```

### 9. Access Dashboard
Open your browser and navigate to:
```
http://localhost:8000/admin
```

## 📦 What's Included

### Resources
- **Users Resource** - Manage system users
- **Posts Resource** - Full content management with rich text editor
- **Categories Resource** - Organize content with categories

### Widgets
- **Stats Overview** - Display key metrics (users, posts, categories count)
- **Latest Posts** - Show recent posts in a table

### Features
- ✅ Rich text editor for content
- ✅ Image uploads and management
- ✅ Auto-generated slugs
- ✅ Publishing workflow
- ✅ Advanced search and filters
- ✅ Bulk actions
- ✅ Responsive design
- ✅ Organized navigation with groups

## 📚 Documentation

For detailed documentation, see:
- **Main Documentation**: `/dashboard/DASHBOARD_README.md`
- **Filament Docs**: https://filamentphp.com/docs
- **Laravel Docs**: https://laravel.com/docs

## 🛠️ Technology Stack

- **Laravel 12** - PHP Framework
- **Filament 4** - Admin Panel
- **PHP 8.2+** - Programming Language
- **SQLite** - Database (easily switchable to MySQL/PostgreSQL)
- **Livewire** - Frontend reactivity
- **Tailwind CSS** - Styling

## 📸 Screenshots

After logging in, you'll see:
- Dashboard with statistics widgets
- Posts management interface with rich editor
- Categories management
- User management

## 🔒 Security Notes

- Change default passwords in production
- Use strong passwords for admin accounts
- Keep Laravel and Filament packages updated
- Use environment variables for sensitive configuration

## 🆘 Troubleshooting

### "Class not found" errors
```bash
composer dump-autoload
php artisan clear-compiled
```

### Permission issues
```bash
chmod -R 775 storage bootstrap/cache
```

### Database locked (SQLite)
Make sure no other process is accessing the database file.

## 📝 Next Steps

1. **Customize branding** - Edit `app/Providers/Filament/AdminPanelProvider.php`
2. **Add more resources** - `php artisan make:filament-resource ResourceName`
3. **Create widgets** - `php artisan make:filament-widget WidgetName`
4. **Add roles & permissions** - Install Spatie Laravel Permission
5. **Deploy** - Configure for production environment

## 🤝 Contributing

Feel free to customize and extend the dashboard for your specific needs.

## 📄 License

Built with open-source technologies under MIT licenses.
