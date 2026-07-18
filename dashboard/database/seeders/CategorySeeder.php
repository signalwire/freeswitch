<?php

namespace Database\Seeders;

use App\Models\Category;
use Illuminate\Database\Console\Seeds\WithoutModelEvents;
use Illuminate\Database\Seeder;
use Illuminate\Support\Str;

class CategorySeeder extends Seeder
{
    /**
     * Run the database seeds.
     */
    public function run(): void
    {
        $categories = [
            [
                'name' => 'Technology',
                'slug' => 'technology',
                'description' => 'Articles about technology, programming, and software development.',
                'is_visible' => true,
            ],
            [
                'name' => 'Business',
                'slug' => 'business',
                'description' => 'Business insights, strategies, and trends.',
                'is_visible' => true,
            ],
            [
                'name' => 'Lifestyle',
                'slug' => 'lifestyle',
                'description' => 'Lifestyle tips, wellness, and personal development.',
                'is_visible' => true,
            ],
            [
                'name' => 'Travel',
                'slug' => 'travel',
                'description' => 'Travel guides, destinations, and adventures.',
                'is_visible' => true,
            ],
            [
                'name' => 'Food',
                'slug' => 'food',
                'description' => 'Recipes, restaurant reviews, and culinary adventures.',
                'is_visible' => true,
            ],
        ];

        foreach ($categories as $category) {
            Category::create($category);
        }
    }
}
