<?php

namespace Database\Seeders;

use App\Models\Category;
use App\Models\Post;
use App\Models\User;
use Illuminate\Database\Console\Seeds\WithoutModelEvents;
use Illuminate\Database\Seeder;
use Illuminate\Support\Str;

class PostSeeder extends Seeder
{
    /**
     * Run the database seeds.
     */
    public function run(): void
    {
        $user = User::first();
        $categories = Category::all();

        if (!$user || $categories->isEmpty()) {
            $this->command->error('Please seed users and categories first!');
            return;
        }

        $posts = [
            [
                'title' => 'Getting Started with Laravel',
                'content' => '<h2>Introduction to Laravel</h2><p>Laravel is a powerful PHP framework that makes web development elegant and enjoyable. In this post, we will explore the basic concepts of Laravel and how to get started with your first project.</p><p>Laravel provides an expressive, elegant syntax and includes features like routing, authentication, sessions, and caching out of the box.</p>',
                'excerpt' => 'Learn the basics of Laravel, a powerful PHP framework for web development.',
                'category_id' => $categories->where('slug', 'technology')->first()->id ?? $categories->random()->id,
                'is_published' => true,
                'published_at' => now()->subDays(5),
            ],
            [
                'title' => '10 Tips for Remote Work Success',
                'content' => '<h2>Master Remote Work</h2><p>Working remotely has become the new normal. Here are 10 essential tips to help you succeed while working from home.</p><p>From setting up a dedicated workspace to maintaining work-life balance, we cover everything you need to know.</p>',
                'excerpt' => 'Discover proven strategies for succeeding in a remote work environment.',
                'category_id' => $categories->where('slug', 'business')->first()->id ?? $categories->random()->id,
                'is_published' => true,
                'published_at' => now()->subDays(3),
            ],
            [
                'title' => 'The Art of Mindful Living',
                'content' => '<h2>Embrace Mindfulness</h2><p>Mindfulness is more than just a buzzword. It\'s a practice that can transform your daily life and improve your overall well-being.</p><p>Learn practical techniques to incorporate mindfulness into your everyday routine.</p>',
                'excerpt' => 'Transform your life through the practice of mindfulness.',
                'category_id' => $categories->where('slug', 'lifestyle')->first()->id ?? $categories->random()->id,
                'is_published' => true,
                'published_at' => now()->subDays(7),
            ],
            [
                'title' => 'Top 5 Hidden Gems in Europe',
                'content' => '<h2>Undiscovered European Destinations</h2><p>Skip the crowded tourist hotspots and explore these hidden gems across Europe. From charming villages to breathtaking landscapes, these destinations offer authentic experiences.</p>',
                'excerpt' => 'Explore lesser-known but stunning destinations across Europe.',
                'category_id' => $categories->where('slug', 'travel')->first()->id ?? $categories->random()->id,
                'is_published' => true,
                'published_at' => now()->subDays(2),
            ],
            [
                'title' => 'Mastering Italian Pasta from Scratch',
                'content' => '<h2>Homemade Italian Pasta</h2><p>Making pasta from scratch is easier than you think! Follow this comprehensive guide to create authentic Italian pasta in your own kitchen.</p><p>We\'ll cover everything from choosing the right flour to achieving the perfect texture.</p>',
                'excerpt' => 'Learn how to make authentic Italian pasta from scratch at home.',
                'category_id' => $categories->where('slug', 'food')->first()->id ?? $categories->random()->id,
                'is_published' => true,
                'published_at' => now()->subDay(),
            ],
            [
                'title' => 'Building Modern Web Applications',
                'content' => '<h2>Modern Web Development</h2><p>Explore the latest technologies and best practices for building modern web applications. From frontend frameworks to backend architectures.</p>',
                'excerpt' => 'A comprehensive guide to modern web application development.',
                'category_id' => $categories->where('slug', 'technology')->first()->id ?? $categories->random()->id,
                'is_published' => false,
                'published_at' => null,
            ],
            [
                'title' => 'Scaling Your Startup',
                'content' => '<h2>Growth Strategies</h2><p>Learn proven strategies for scaling your startup from early stage to sustainable growth. Real insights from successful entrepreneurs.</p>',
                'excerpt' => 'Essential strategies for taking your startup to the next level.',
                'category_id' => $categories->where('slug', 'business')->first()->id ?? $categories->random()->id,
                'is_published' => false,
                'published_at' => null,
            ],
        ];

        foreach ($posts as $postData) {
            $postData['slug'] = Str::slug($postData['title']);
            $postData['user_id'] = $user->id;
            Post::create($postData);
        }

        $this->command->info('Posts seeded successfully!');
    }
}
