# Filament 4 Dashboard

This is a complete admin dashboard built with Laravel 12 and Filament 4.

## Features

- **User Management**: Manage users with full CRUD operations
- **Content Management**: 
  - Posts management with rich text editor
  - Category management
  - Image uploads
  - Publishing workflow
- **Dashboard Widgets**:
  - Statistics overview (users, posts, categories)
  - Latest posts table widget
- **Modern UI**: Built with Filament 4's beautiful interface
- **Search & Filters**: Advanced search and filtering capabilities
- **Responsive Design**: Works on all devices

## Installation & Setup

### Prerequisites

- PHP 8.2 or higher
- Composer
- SQLite (or other database)

### Getting Started

1. **Navigate to the dashboard directory**:
   ```bash
   cd dashboard
   ```

2. **Install dependencies** (if not already installed):
   ```bash
   composer install
   ```

3. **Set up environment**:
   ```bash
   cp .env.example .env
   php artisan key:generate
   ```

4. **Create database**:
   ```bash
   touch database/database.sqlite
   ```

5. **Run migrations**:
   ```bash
   php artisan migrate
   ```

6. **Create an admin user**:
   ```bash
   php artisan make:filament-user
   ```
   Follow the prompts to create your admin account.

7. **Start the development server**:
   ```bash
   php artisan serve
   ```

8. **Access the dashboard**:
   Open your browser and go to: `http://localhost:8000/admin`

## Default Admin Credentials

The admin user created during setup:
- **Email**: admin@example.com
- **Password**: password (change this in production!)

## Resources

### Posts Resource
- **Location**: `/admin/posts`
- **Features**:
  - Rich text editor for content
  - Category association
  - Featured image upload
  - Publishing status
  - Publish date scheduling
  - Auto-generated slugs
  - Search and filters

### Categories Resource
- **Location**: `/admin/categories`
- **Features**:
  - Category name and slug
  - Description
  - Visibility toggle
  - Post count display
  - Auto-generated slugs

### Users Resource
- **Location**: `/admin/users`
- **Features**:
  - User management
  - Email verification
  - Role-based access (extensible)

## Dashboard Widgets

### Stats Overview
Displays key metrics:
- Total users
- Total posts
- Published posts
- Total categories

### Latest Posts
Shows the 5 most recent posts with:
- Title
- Author
- Category
- Published status
- Creation date

## Customization

### Adding New Resources

```bash
php artisan make:filament-resource ResourceName --generate
```

### Adding New Widgets

```bash
php artisan make:filament-widget WidgetName --stats-overview
```

### Customizing Colors

Edit `app/Providers/Filament/AdminPanelProvider.php`:

```php
->colors([
    'primary' => Color::Blue,
])
```

### Navigation Groups

Resources are organized into groups:
- **Content Management**: Posts, Categories
- **User Management**: Users
- **Settings**: (for future additions)

## File Structure

```
app/
├── Filament/
│   ├── Resources/
│   │   ├── Posts/
│   │   │   ├── Pages/
│   │   │   ├── Schemas/
│   │   │   ├── Tables/
│   │   │   └── PostResource.php
│   │   ├── Categories/
│   │   └── Users/
│   ├── Widgets/
│   │   ├── StatsOverview.php
│   │   └── LatestPosts.php
│   └── Pages/
└── Providers/
    └── Filament/
        └── AdminPanelProvider.php
```

## Database Schema

### Posts Table
- id
- title
- slug
- category_id (foreign key)
- user_id (foreign key)
- content (text)
- excerpt (text)
- featured_image
- is_published (boolean)
- published_at (timestamp)
- timestamps

### Categories Table
- id
- name
- slug
- description (text)
- is_visible (boolean)
- timestamps

### Users Table
- id
- name
- email
- email_verified_at
- password
- remember_token
- timestamps

## Tips

1. **Auto-generate slugs**: Slugs are automatically generated from titles when creating new posts/categories
2. **Image uploads**: Images are stored in the `storage/app/public` directory
3. **Rich text editor**: Supports formatting, links, and file attachments
4. **Bulk actions**: Select multiple records to perform bulk operations
5. **Column toggles**: Hide/show columns in table views
6. **Export**: Use Filament's export features to download data

## Security

- Always change default passwords in production
- Use strong passwords for admin accounts
- Enable two-factor authentication (can be added as a plugin)
- Keep Laravel and Filament updated
- Use environment variables for sensitive data

## Support

For more information about Filament, visit:
- [Filament Documentation](https://filamentphp.com/docs)
- [Laravel Documentation](https://laravel.com/docs)

## License

This dashboard is built using open-source technologies:
- Laravel Framework (MIT License)
- Filament Admin Panel (MIT License)
